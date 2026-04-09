#include "mainwindow.h"

#include "dialogs/aboutdialog.h"
#include "dialogs/newprojectdialog.h"
#include "theme/themehelper.h"
#include "widgets/menubutton.h"

#include <QCloseEvent>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QMessageBox>
#include <QPixmap>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <cstring>

MainWindow::MainWindow(const QString& startupProjectFile, QWidget* parent)
    : QMainWindow(parent), m_appSettings("Atom", "Dsa16ChAcquisition") {
    m_projectManager = new ProjectManager(this);
    m_dllLoader = new Dsa16ChDllLoader(this);
    m_deviceService = new Dsa16ChDeviceService(m_dllLoader, this);
    m_dataStore = new MultiChannelDataStore(65536, this);
    m_acquisitionService = new AcquisitionService(m_deviceService, m_dataStore, this);
    m_receiverService = new AcquisitionTcpReceiverService(m_dataStore, this);
    m_plotService = new PlotService(this);

    if (qEnvironmentVariableIsSet("DSA_USE_MOCK")) {
        m_acquisitionService->setMockModeEnabled(true);
    }

    buildUi();
    bindSignals();
    initializeSdk();
    m_acquisitionService->setNetworkSettings(dsa::DsaNetworkSettings{});
    m_receiverService->setReceiverSettings(dsa::DsaReceiverSettings{});
    refreshHomePageProject();
    m_homePage->setRecentProjects(recentProjects());

    if (!startupProjectFile.trimmed().isEmpty()) {
        openProjectFile(startupProjectFile, true);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
    m_plotService->stopPlotting();
    m_acquisitionService->stopAcquisition();
    m_receiverService->stopReceiving();
    m_deviceService->shutdown();
    QMainWindow::closeEvent(event);
}

void MainWindow::onNewProjectClicked() {
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString error;
    if (!m_projectManager->createProject(dialog.projectName(), dialog.projectBaseDirectory(), &error)) {
        showError("新建项目失败", error);
        return;
    }

    addRecentProject(m_projectManager->currentProjectFilePath());
    m_homePage->setRecentProjects(recentProjects());
    m_stack->setCurrentWidget(m_homePage);
    statusBar()->showMessage("项目创建并打开成功。", 4000);
}

void MainWindow::onOpenProjectClicked() {
    const QString filePath =
        QFileDialog::getOpenFileName(this, "打开项目", QString(), "Acquisition Project (*.acqproj)");
    if (filePath.isEmpty()) {
        return;
    }
    openProjectFile(filePath, true);
}

void MainWindow::onOpenDeviceClicked() {
    if (currentAppMode() == static_cast<unsigned int>(dsa::AppMode::Receiver)) {
        showError("打开设备失败", "接收端模式下不需要打开本地采集设备。");
        return;
    }
    if (!m_deviceService->sdkReady()) {
        showError("打开设备失败", QString("SDK 未就绪。\n%1").arg(m_deviceService->sdkStatusMessage()));
        return;
    }
    if (!m_deviceService->openDevice()) {
        showError("打开设备失败", m_deviceService->lastError());
        updateDeviceUi(false);
        return;
    }
    updateDeviceUi(true);
    statusBar()->showMessage("设备已打开。", 3000);
}

void MainWindow::onShowSettingsPageClicked() {
    m_stack->setCurrentWidget(m_settingsPage);
}

void MainWindow::onShowAcquisitionPageClicked() {
    m_stack->setCurrentWidget(m_acquisitionPage);
}

void MainWindow::onToggleAcquisitionClicked() {
    if (currentAppMode() == static_cast<unsigned int>(dsa::AppMode::Sender) && m_acquisitionService->isRunning()) {
        m_acquisitionService->stopAcquisition();
        return;
    }
    if (currentAppMode() == static_cast<unsigned int>(dsa::AppMode::Receiver) && m_receiverService->isRunning()) {
        m_receiverService->stopReceiving();
        return;
    }
    if (currentAppMode() == static_cast<unsigned int>(dsa::AppMode::Receiver) && m_acquisitionService->isRunning()) {
        m_acquisitionService->stopAcquisition();
    }
    if (currentAppMode() == static_cast<unsigned int>(dsa::AppMode::Sender) && m_receiverService->isRunning()) {
        m_receiverService->stopReceiving();
    }

    dsa::DsaAcquisitionSettings settings;
    dsa::DsaNetworkSettings networkSettings;
    dsa::DsaReceiverSettings receiverSettings;
    QString validationError;
    if (!m_settingsPage->collectSettings(settings, &validationError)) {
        showError("参数错误", validationError);
        return;
    }
    if (!m_settingsPage->collectNetworkSettings(networkSettings, &validationError)) {
        showError("参数错误", validationError);
        return;
    }
    if (!m_settingsPage->collectReceiverSettings(receiverSettings, &validationError)) {
        showError("参数错误", validationError);
        return;
    }

    m_acquisitionPage->setAppMode(settings.appMode);

    if (settings.appMode == static_cast<unsigned int>(dsa::AppMode::Receiver)) {
        QString dataDir;
        if (receiverSettings.saveToDisk) {
            QString dataDirError;
            dataDir = resolveAcquisitionDataDirectory(&dataDirError);
            if (dataDir.isEmpty()) {
                showError("开始接收失败", dataDirError);
                return;
            }
        }
        m_receiverService->setReceiverSettings(receiverSettings);
        m_receiverService->setDataDirectory(dataDir);
        if (!m_receiverService->startReceiving()) {
            showError("开始接收失败", m_receiverService->statusMessage());
            return;
        }
        statusBar()->showMessage(QString("接收端已启动，监听 %1:%2")
                                     .arg(receiverSettings.listenHost)
                                     .arg(receiverSettings.listenPort),
                                 5000);
        return;
    }

    if (!settings.saveToDisk && !networkSettings.enabled) {
        const QMessageBox::StandardButton choice = QMessageBox::question(
            this,
            "确认开始采集",
            "当前已关闭本地落盘，且未启用网络传输。\n采集数据将只保留在内存中，停止后无法恢复。\n是否仍然继续开始采集？",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            statusBar()->showMessage("已取消开始采集。", 3000);
            return;
        }
    }

    if (m_deviceService->isDeviceOpen()) {
        if (!m_deviceService->setAcquisitionSettings(settings)) {
            showError("应用采集参数失败", m_deviceService->lastError());
            return;
        }
    } else if (!m_acquisitionService->isMockModeEnabled()) {
        showError("开始采集失败", "设备未打开，且未启用 mock 模式。");
        return;
    }

    m_acquisitionService->setAcquisitionSettings(settings);
    m_acquisitionService->setNetworkSettings(networkSettings);
    QString dataDir;
    if (settings.saveToDisk) {
        QString dataDirError;
        dataDir = resolveAcquisitionDataDirectory(&dataDirError);
        if (dataDir.isEmpty()) {
            showError("开始采集失败", dataDirError);
            return;
        }
    }
    m_acquisitionService->setDataDirectory(dataDir);

    if (!m_acquisitionService->startAcquisition()) {
        showError("开始采集失败", m_acquisitionService->lastError());
        return;
    }
    if (settings.saveToDisk && !m_projectManager->hasCurrentProject()) {
        statusBar()->showMessage(QString("未打开项目，数据将保存到：%1").arg(dataDir), 6000);
    } else if (!settings.saveToDisk) {
        statusBar()->showMessage("采集已开始，当前不落盘，仅保留内存数据并按网络设置发送。", 5000);
    }
}

void MainWindow::onTogglePlotClicked() {
    if (m_plotService->isRunning()) {
        m_plotService->stopPlotting();
        return;
    }

    if (!m_acquisitionService->isRunning() && !m_receiverService->isRunning()) {
        const bool loaded = loadHistoryDataToStore(4096);
        if (!loaded) {
            statusBar()->showMessage("未检测到历史数据，绘图将等待实时数据。", 5000);
        }
    }

    m_plotService->startPlotting(50);
}

void MainWindow::onQuickStartClicked() {
    if (currentAppMode() == static_cast<unsigned int>(dsa::AppMode::Receiver)) {
        onToggleAcquisitionClicked();
        return;
    }
    if (m_acquisitionService->isRunning()) {
        m_stack->setCurrentWidget(m_acquisitionPage);
        statusBar()->showMessage("采集已在运行。", 3000);
        return;
    }

    const dsa::DsaAcquisitionSettings defaultSettings{};
    const dsa::DsaNetworkSettings defaultNetworkSettings{};
    QString validationError;
    if (!defaultSettings.validate(&validationError)) {
        showError("一键启动失败", QString("默认参数无效：%1").arg(validationError));
        return;
    }
    if (!defaultNetworkSettings.validate(&validationError)) {
        showError("一键启动失败", QString("默认网络参数无效：%1").arg(validationError));
        return;
    }

    const bool hasProject = m_projectManager->hasCurrentProject();
    if (!m_deviceService->sdkReady()) {
        showError("一键启动失败", QString("SDK 未就绪。\n%1").arg(m_deviceService->sdkStatusMessage()));
        return;
    }

    if (!m_deviceService->isDeviceOpen()) {
        if (!m_deviceService->openDevice()) {
            showError("一键启动失败", QString("打开设备失败：%1").arg(m_deviceService->lastError()));
            return;
        }
        updateDeviceUi(true);
    }

    if (!m_deviceService->setAcquisitionSettings(defaultSettings)) {
        showError("一键启动失败", QString("下发默认参数失败：%1").arg(m_deviceService->lastError()));
        return;
    }

    m_settingsPage->setSettings(defaultSettings);
    m_settingsPage->setNetworkSettings(defaultNetworkSettings);
    m_acquisitionService->setAcquisitionSettings(defaultSettings);
    m_acquisitionService->setNetworkSettings(defaultNetworkSettings);
    m_acquisitionPage->setAppMode(defaultSettings.appMode);
    m_settingsPage->setStatusMessage("已应用默认参数并启动采集流程。");

    QString dataDir;
    if (defaultSettings.saveToDisk) {
        QString dataDirError;
        dataDir = resolveAcquisitionDataDirectory(&dataDirError);
        if (dataDir.isEmpty()) {
            showError("一键启动失败", dataDirError);
            return;
        }
    }
    m_acquisitionService->setDataDirectory(dataDir);

    if (!m_acquisitionService->startAcquisition()) {
        showError("一键启动失败", QString("启动采集失败：%1").arg(m_acquisitionService->lastError()));
        return;
    }

    m_stack->setCurrentWidget(m_acquisitionPage);
    if (hasProject) {
        statusBar()->showMessage("一键启动完成：设备已打开，默认参数已应用，采集已开始。", 4000);
    } else if (defaultSettings.saveToDisk) {
        statusBar()->showMessage(QString("一键启动完成：已开始采集，数据保存到：%1").arg(dataDir), 6000);
    } else {
        statusBar()->showMessage("一键启动完成：已开始采集，当前不落盘。", 4000);
    }
}

void MainWindow::onQuickClearClicked() {
    m_manualNetworkConnectPending = false;
    m_plotService->stopPlotting();
    m_acquisitionService->stopAcquisition();
    m_receiverService->stopReceiving();
    m_deviceService->shutdown();
    m_deviceService->resetCachedState();
    initializeSdk();

    m_dataStore->clear();
    m_acquisitionService->setDataDirectory(QString());

    const dsa::DsaAcquisitionSettings defaultSettings{};
    const dsa::DsaNetworkSettings defaultNetworkSettings{};
    const dsa::DsaReceiverSettings defaultReceiverSettings{};
    m_settingsPage->setSettings(defaultSettings);
    m_settingsPage->setNetworkSettings(defaultNetworkSettings);
    m_settingsPage->setReceiverSettings(defaultReceiverSettings);
    m_settingsPage->setDioInputValue(0x00);
    m_settingsPage->setStatusMessage("已恢复默认状态。");
    m_acquisitionService->setAcquisitionSettings(defaultSettings);
    m_acquisitionService->setNetworkSettings(defaultNetworkSettings);
    m_receiverService->setReceiverSettings(defaultReceiverSettings);
    m_receiverService->setDataDirectory(QString());

    m_acquisitionPage->setAppMode(defaultSettings.appMode);
    m_acquisitionPage->setSelectedChannels(QVector<int>{0, 1, 2, 3});
    m_acquisitionPage->setAcquisitionRunning(false);
    m_acquisitionPage->setPlottingRunning(false);
    m_acquisitionPage->setNetworkState(false, false);
    m_acquisitionPage->setOverflow(false);
    m_acquisitionPage->setBufferPointCount(0);
    m_acquisitionPage->clearWaveforms();

    m_projectManager->clearCurrentProject();
    refreshHomePageProject();
    m_homePage->setRecentProjects(recentProjects());

    updateDeviceUi(false);
    updateAcquisitionUi(false);
    updatePlotUi(false);

    m_stack->setCurrentWidget(m_homePage);
    statusBar()->showMessage("软件已恢复到初始状态。", 4000);
}

void MainWindow::onShowAboutClicked() {
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onReadSettingsRequested() {
    if (currentAppMode() == static_cast<unsigned int>(dsa::AppMode::Receiver)) {
        m_settingsPage->setStatusMessage("接收端模式下没有本地设备参数可读取。");
        return;
    }
    m_settingsPage->setSettings(m_deviceService->currentSettings());
    m_settingsPage->setStatusMessage("已加载当前缓存配置。");
}

void MainWindow::onApplySettingsRequested(const dsa::DsaAcquisitionSettings& settings,
                                          const dsa::DsaNetworkSettings& networkSettings,
                                          const dsa::DsaReceiverSettings& receiverSettings) {
    m_manualNetworkConnectPending = false;
    m_acquisitionPage->setAppMode(settings.appMode);
    m_acquisitionService->setAcquisitionSettings(settings);
    m_acquisitionService->setNetworkSettings(networkSettings);
    m_receiverService->setReceiverSettings(receiverSettings);
    updateDeviceUi(m_deviceService->isDeviceOpen());
    if (settings.appMode == static_cast<unsigned int>(dsa::AppMode::Receiver)) {
        m_settingsPage->setStatusMessage("接收端参数已应用。");
        updateAcquisitionUi(m_receiverService->isRunning());
        updateDeviceUi(false);
        return;
    }
    if (!m_deviceService->isDeviceOpen()) {
        m_settingsPage->setStatusMessage("设备未打开，采集参数和网络参数已保存为本地待应用配置。");
        updateAcquisitionUi(m_acquisitionService->isRunning());
        return;
    }
    if (!m_deviceService->setAcquisitionSettings(settings)) {
        m_settingsPage->setStatusMessage(m_deviceService->lastError(), true);
        showError("应用参数失败", m_deviceService->lastError());
        return;
    }
    m_settingsPage->setStatusMessage("采集参数已下发到设备，网络参数已应用。");
    updateAcquisitionUi(m_acquisitionService->isRunning());
}

void MainWindow::onConnectNetworkRequested(const dsa::DsaNetworkSettings& networkSettings) {
    if (!networkSettings.enabled) {
        m_manualNetworkConnectPending = false;
        m_settingsPage->setStatusMessage("请先启用网络传输。", true);
        statusBar()->showMessage("请先启用网络传输。", 4000);
        return;
    }

    m_manualNetworkConnectPending = true;
    m_acquisitionService->setNetworkSettings(networkSettings);
    if (!m_acquisitionService->connectNetwork(true)) {
        m_manualNetworkConnectPending = false;
        const QString message = m_acquisitionService->networkStatusMessage().trimmed().isEmpty()
                                    ? QString("发起网络连接失败。")
                                    : m_acquisitionService->networkStatusMessage();
        m_settingsPage->setStatusMessage(message, true);
        statusBar()->showMessage(message, 5000);
        showError("网络连接失败", message);
        return;
    }

    if (m_acquisitionService->isNetworkConnected()) {
        m_manualNetworkConnectPending = false;
        const QString message = m_acquisitionService->networkStatusMessage().trimmed().isEmpty()
                                    ? QString("网络已连接。")
                                    : m_acquisitionService->networkStatusMessage();
        m_settingsPage->setStatusMessage(message);
        statusBar()->showMessage(message, 5000);
        QMessageBox::information(this, "网络连接", message);
        return;
    }

    m_settingsPage->setStatusMessage("正在连接网络目标...");
    statusBar()->showMessage("正在连接网络目标...", 5000);
}

void MainWindow::onSaveToProjectRequested(const dsa::DsaAcquisitionSettings& settings,
                                          const dsa::DsaDioSettings& dioSettings,
                                          const dsa::DsaNetworkSettings& networkSettings,
                                          const dsa::DsaReceiverSettings& receiverSettings) {
    if (!m_projectManager->hasCurrentProject()) {
        showError("保存失败", "当前没有打开的项目。");
        return;
    }

    QString error;
    if (!m_projectManager->updateAcquisitionSettings(settings.toJson(), &error)) {
        showError("保存失败", error);
        return;
    }
    if (!m_projectManager->updateNetworkSettings(networkSettings.toJson(), &error)) {
        showError("保存失败", error);
        return;
    }
    if (!m_projectManager->updateReceiverSettings(receiverSettings.toJson(), &error)) {
        showError("保存失败", error);
        return;
    }
    if (!m_projectManager->updateDeviceSettings(dioSettings.toJson(), &error)) {
        showError("保存失败", error);
        return;
    }
    m_settingsPage->setStatusMessage("采集参数、网络参数和 DIO 参数已保存到项目文件。");
    statusBar()->showMessage("项目参数已保存。", 3000);
}

void MainWindow::onDioDirectionSetRequested(unsigned int groupIndex, unsigned int direction) {
    if (!m_deviceService->isDeviceOpen()) {
        showError("DIO 操作失败", "设备未打开。");
        return;
    }
    if (!m_deviceService->setDioDirection(groupIndex, direction)) {
        showError("DIO 操作失败", m_deviceService->lastError());
        return;
    }
    m_settingsPage->setStatusMessage(QString("DIO 方向设置成功：group %1 -> %2").arg(groupIndex).arg(direction));
}

void MainWindow::onDioWriteRequested(unsigned int groupIndex, unsigned char doData) {
    if (!m_deviceService->isDeviceOpen()) {
        showError("DIO 操作失败", "设备未打开。");
        return;
    }
    if (!m_deviceService->writeDo(groupIndex, doData)) {
        showError("DIO 操作失败", m_deviceService->lastError());
        return;
    }
    m_settingsPage->setStatusMessage(
        QString("DO 写入成功：group %1, data=0x%2").arg(groupIndex).arg(QString::number(doData, 16).toUpper()));
}

void MainWindow::onDioReadRequested(unsigned int groupIndex) {
    if (!m_deviceService->isDeviceOpen()) {
        showError("DIO 操作失败", "设备未打开。");
        return;
    }
    unsigned char diData = 0;
    if (!m_deviceService->readDi(groupIndex, diData)) {
        showError("DIO 操作失败", m_deviceService->lastError());
        return;
    }
    m_settingsPage->setDioInputValue(diData);
    m_settingsPage->setStatusMessage(
        QString("DI 读取成功：group %1, data=0x%2").arg(groupIndex).arg(QString::number(diData, 16).toUpper()));
}

void MainWindow::buildUi() {
    setWindowTitle("16通道动态信号采集与波形绘图");
    resize(1400, 860);

    m_centralRoot = new QWidget(this);
    m_centralRoot->setObjectName("RootWindow");
    setCentralWidget(m_centralRoot);

    QHBoxLayout* rootLayout = new QHBoxLayout(m_centralRoot);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(14);

    QWidget* sideMenu = new QWidget(m_centralRoot);
    sideMenu->setObjectName("SideMenu");
    sideMenu->setFixedWidth(141);
    ThemeHelper::applyCardShadow(sideMenu);
    QVBoxLayout* sideLayout = new QVBoxLayout(sideMenu);
    sideLayout->setContentsMargins(10, 16, 10, 16);
    sideLayout->setSpacing(10);

    QLabel* appTitle = new QLabel("16CH DSA", sideMenu);
    appTitle->setObjectName("AppTitle");
    appTitle->setAlignment(Qt::AlignCenter);
    QLabel* appSubTitle = new QLabel(sideMenu);
    appSubTitle->setObjectName("AppSubTitle");
    const QPixmap subTitleImage(":/brand/cumtb2.png");
    if (!subTitleImage.isNull()) {
        appSubTitle->setPixmap(subTitleImage.scaled(118, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        appSubTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    } else {
        appSubTitle->setText("Industrial Acquisition");
    }
    sideLayout->addWidget(appTitle);
    sideLayout->addWidget(appSubTitle);

    m_newProjectButton = createMenuButton("新建项目");
    m_openProjectButton = createMenuButton("打开项目");
    m_openDeviceButton = createMenuButton("打开设备");
    m_settingsPageButton = createMenuButton("参数设置");
    m_acquisitionPageButton = createMenuButton("采集页面");
    m_toggleAcqButton = createMenuButton("开始采集");
    m_togglePlotButton = createMenuButton("开始绘图");
    m_quickStartButton = createMenuButton("一键启动");
    m_quickClearButton = createMenuButton("一键清空");
    m_aboutButton = createMenuButton("软件信息");

    const QSize menuIconSize(18, 18);
    auto applyMenuIcon = [&](QPushButton* button, const QString& iconPath) {
        button->setIcon(QIcon(iconPath));
        button->setIconSize(menuIconSize);
    };
    applyMenuIcon(m_newProjectButton, ":/menu-icons/folder-add_line.png");
    applyMenuIcon(m_openProjectButton, ":/menu-icons/folder-open_line.png");
    applyMenuIcon(m_openDeviceButton, ":/menu-icons/monitor_line.png");
    applyMenuIcon(m_settingsPageButton, ":/menu-icons/setting_line.png");
    applyMenuIcon(m_acquisitionPageButton, ":/menu-icons/data-monitor_line.png");
    applyMenuIcon(m_toggleAcqButton, ":/menu-icons/synchronize_line.png");
    applyMenuIcon(m_togglePlotButton, ":/menu-icons/line-chart_line.png");
    applyMenuIcon(m_quickStartButton, ":/menu-icons/check-circle_line.png");
    applyMenuIcon(m_quickClearButton, ":/menu-icons/close-circle_line.png");
    applyMenuIcon(m_aboutButton, ":/menu-icons/question-circle_line.png");

    sideLayout->addSpacing(10);
    sideLayout->addWidget(m_newProjectButton);
    sideLayout->addWidget(m_openProjectButton);
    sideLayout->addWidget(m_openDeviceButton);
    sideLayout->addWidget(m_settingsPageButton);
    sideLayout->addWidget(m_acquisitionPageButton);
    sideLayout->addWidget(m_toggleAcqButton);
    sideLayout->addWidget(m_togglePlotButton);
    sideLayout->addStretch();
    sideLayout->addWidget(m_quickStartButton);
    sideLayout->addWidget(m_quickClearButton);
    sideLayout->addWidget(m_aboutButton);
    rootLayout->addWidget(sideMenu);

    QWidget* rightArea = new QWidget(m_centralRoot);
    rightArea->setObjectName("WorkArea");
    QVBoxLayout* rightLayout = new QVBoxLayout(rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(14);

    m_stack = new QStackedWidget(rightArea);
    m_homePage = new HomePage(m_stack);
    m_settingsPage = new SettingsPage(m_stack);
    m_acquisitionPage = new AcquisitionPage(m_dataStore, m_stack);
    m_historyPlaceholderPage = new QWidget(m_stack);
    {
        QVBoxLayout* layout = new QVBoxLayout(m_historyPlaceholderPage);
        QWidget* card = new QWidget(m_historyPlaceholderPage);
        card->setObjectName("CardWidget");
        ThemeHelper::applyCardShadow(card);
        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        QLabel* label = new QLabel("历史数据页面预留（可扩展文件浏览、回放和导出）。", card);
        label->setObjectName("HintLabel");
        cardLayout->addWidget(label);
        layout->addWidget(card);
    }

    m_stack->addWidget(m_homePage);
    m_stack->addWidget(m_settingsPage);
    m_stack->addWidget(m_acquisitionPage);
    m_stack->addWidget(m_historyPlaceholderPage);
    m_stack->setCurrentWidget(m_homePage);
    rightLayout->addWidget(m_stack, 1);

    rootLayout->addWidget(rightArea, 1);

}

QPushButton* MainWindow::createMenuButton(const QString& text) {
    auto* button = new MenuButton(text, this);
    button->setObjectName("SideMenuButton");
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(40);
    button->setLayoutDirection(Qt::LeftToRight);
    button->setLeftIconPadding(6);
    button->setRightTextPadding(8);
    button->setMiddleGap(12);
    return button;
}

void MainWindow::bindSignals() {
    connect(m_newProjectButton, &QPushButton::clicked, this, &MainWindow::onNewProjectClicked);
    connect(m_openProjectButton, &QPushButton::clicked, this, &MainWindow::onOpenProjectClicked);
    connect(m_openDeviceButton, &QPushButton::clicked, this, &MainWindow::onOpenDeviceClicked);
    connect(m_settingsPageButton, &QPushButton::clicked, this, &MainWindow::onShowSettingsPageClicked);
    connect(m_acquisitionPageButton, &QPushButton::clicked, this, &MainWindow::onShowAcquisitionPageClicked);
    connect(m_toggleAcqButton, &QPushButton::clicked, this, &MainWindow::onToggleAcquisitionClicked);
    connect(m_togglePlotButton, &QPushButton::clicked, this, &MainWindow::onTogglePlotClicked);
    connect(m_quickStartButton, &QPushButton::clicked, this, &MainWindow::onQuickStartClicked);
    connect(m_quickClearButton, &QPushButton::clicked, this, &MainWindow::onQuickClearClicked);
    connect(m_aboutButton, &QPushButton::clicked, this, &MainWindow::onShowAboutClicked);

    connect(m_projectManager, &ProjectManager::projectOpened, this, [this](const ProjectFileModel& project) {
        addRecentProject(project.projectFilePath);
        refreshHomePageProject();
        m_homePage->setRecentProjects(recentProjects());
        m_acquisitionService->setDataDirectory(project.dataPath);

        if (!project.acquisitionSettings.isEmpty()) {
            const dsa::DsaAcquisitionSettings settings = dsa::DsaAcquisitionSettings::fromJson(project.acquisitionSettings);
            m_settingsPage->setSettings(settings);
            m_acquisitionService->setAcquisitionSettings(settings);
            m_acquisitionPage->setAppMode(settings.appMode);
        }
        const dsa::DsaNetworkSettings networkSettings = dsa::DsaNetworkSettings::fromJson(project.networkSettings);
        m_settingsPage->setNetworkSettings(networkSettings);
        m_acquisitionService->setNetworkSettings(networkSettings);
        const dsa::DsaReceiverSettings receiverSettings = dsa::DsaReceiverSettings::fromJson(project.receiverSettings);
        m_settingsPage->setReceiverSettings(receiverSettings);
        m_receiverService->setReceiverSettings(receiverSettings);
        updateDeviceUi(m_deviceService->isDeviceOpen());
        updateAcquisitionUi(false);
    });

    connect(m_deviceService, &Dsa16ChDeviceService::sdkStateChanged, this, &MainWindow::updateSdkUi);
    connect(m_deviceService, &Dsa16ChDeviceService::deviceOpenStateChanged, this, &MainWindow::updateDeviceUi);

    connect(m_settingsPage, &SettingsPage::appModeChanged, this, [this](unsigned int appMode) {
        if (m_acquisitionService->isRunning() || m_receiverService->isRunning()) {
            return;
        }
        m_acquisitionPage->setAppMode(appMode);
        updateDeviceUi(m_deviceService->isDeviceOpen());
        updateAcquisitionUi(false);
    });
    connect(m_settingsPage, &SettingsPage::readSettingsRequested, this, &MainWindow::onReadSettingsRequested);
    connect(m_settingsPage, &SettingsPage::applySettingsRequested, this, &MainWindow::onApplySettingsRequested);
    connect(m_settingsPage, &SettingsPage::connectNetworkRequested, this, &MainWindow::onConnectNetworkRequested);
    connect(m_settingsPage, &SettingsPage::saveToProjectRequested, this, &MainWindow::onSaveToProjectRequested);
    connect(m_settingsPage, &SettingsPage::dioDirectionSetRequested, this, &MainWindow::onDioDirectionSetRequested);
    connect(m_settingsPage, &SettingsPage::dioWriteRequested, this, &MainWindow::onDioWriteRequested);
    connect(m_settingsPage, &SettingsPage::dioReadRequested, this, &MainWindow::onDioReadRequested);

    connect(m_acquisitionService, &AcquisitionService::runningChanged, this, &MainWindow::updateAcquisitionUi);
    connect(m_acquisitionService, &AcquisitionService::bufferPointCountUpdated, this, &MainWindow::onBufferPointCountUpdated);
    connect(m_acquisitionService, &AcquisitionService::overflowDetected, m_acquisitionPage, &AcquisitionPage::setOverflow);
    connect(m_acquisitionService, &AcquisitionService::errorOccurred, this, [this](const QString& message) {
        statusBar()->showMessage(message, 5000);
    });

    connect(m_plotService, &PlotService::plottingChanged, this, &MainWindow::updatePlotUi);
    connect(m_plotService, &PlotService::plottingTick, this, &MainWindow::onPlotTick);
    connect(m_acquisitionService,
            &AcquisitionService::networkStateChanged,
            m_acquisitionPage,
            &AcquisitionPage::setNetworkState);
    connect(m_acquisitionService,
            &AcquisitionService::networkStateChanged,
            this,
            &MainWindow::handleNetworkStateChanged);
    connect(m_receiverService, &AcquisitionTcpReceiverService::runningChanged, this, &MainWindow::updateAcquisitionUi);
    connect(m_receiverService,
            &AcquisitionTcpReceiverService::bufferPointCountUpdated,
            this,
            &MainWindow::onBufferPointCountUpdated);
    connect(m_receiverService,
            &AcquisitionTcpReceiverService::networkStateChanged,
            m_acquisitionPage,
            &AcquisitionPage::setNetworkState);
    connect(m_receiverService,
            &AcquisitionTcpReceiverService::errorOccurred,
            this,
            [this](const QString& message) { statusBar()->showMessage(message, 5000); });
}

void MainWindow::initializeSdk() {
    if (!m_deviceService->initializeSdk()) {
        updateSdkUi(false, m_deviceService->lastError());
        return;
    }
    updateSdkUi(true, m_deviceService->sdkStatusMessage());
}

bool MainWindow::openProjectFile(const QString& projectFilePath, bool showMessage) {
    QString error;
    if (!m_projectManager->openProject(projectFilePath, &error)) {
        if (showMessage) {
            showError("打开项目失败", error);
        }
        return false;
    }

    if (showMessage) {
        statusBar()->showMessage(QString("项目已打开：%1").arg(projectFilePath), 4000);
    }
    m_stack->setCurrentWidget(m_homePage);
    return true;
}

bool MainWindow::loadHistoryDataToStore(int maxPointsPerChannel) {
    if (!m_projectManager->hasCurrentProject()) {
        return false;
    }

    const QString dataPath = m_projectManager->currentProject().dataPath;
    QDir dataDir(dataPath);
    if (!dataDir.exists()) {
        return false;
    }

    m_dataStore->clear();
    bool loaded = false;

    auto loadFromSessionBinary = [&]() -> bool {
        const QStringList sessionFiles =
            dataDir.entryList(QStringList() << "session_*.bin" << "session_*.dsa16bin", QDir::Files, QDir::Time);
        if (sessionFiles.isEmpty()) {
            return false;
        }

        QFile file(dataDir.filePath(sessionFiles.first()));
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        auto readExact = [&file](void* out, qint64 size) -> bool {
            const qint64 n = file.read(reinterpret_cast<char*>(out), size);
            return n == size;
        };

        quint32 magic = 0;
        quint32 version = 0;
        quint32 channelCount = 0;
        if (!readExact(&magic, sizeof(magic)) || !readExact(&version, sizeof(version)) ||
            !readExact(&channelCount, sizeof(channelCount))) {
            file.close();
            return false;
        }
        if (magic != 0x31415344 || version != 1 || channelCount != static_cast<quint32>(dsa::kChannelCount)) {
            file.close();
            return false;
        }

        QVector<QVector<double>> channels(dsa::kChannelCount);
        bool hasFrame = false;
        while (!file.atEnd()) {
            quint64 timestampMs = 0;
            quint32 pointCount = 0;
            quint32 frameChannelCount = 0;
            if (!readExact(&timestampMs, sizeof(timestampMs))) {
                break;
            }
            if (!readExact(&pointCount, sizeof(pointCount)) || !readExact(&frameChannelCount, sizeof(frameChannelCount))) {
                break;
            }
            if (frameChannelCount != static_cast<quint32>(dsa::kChannelCount) || pointCount == 0 ||
                pointCount > dsa::kMaxPointPerChannel) {
                break;
            }

            const qint64 bytesPerChannel = static_cast<qint64>(pointCount) * static_cast<qint64>(sizeof(double));
            for (int ch = 0; ch < dsa::kChannelCount; ++ch) {
                const QByteArray raw = file.read(bytesPerChannel);
                if (raw.size() != bytesPerChannel) {
                    file.close();
                    return false;
                }

                QVector<double>& series = channels[ch];
                const int oldSize = series.size();
                series.resize(oldSize + static_cast<int>(pointCount));
                memcpy(series.data() + oldSize, raw.constData(), static_cast<size_t>(bytesPerChannel));

                if (series.size() > maxPointsPerChannel) {
                    const int extra = series.size() - maxPointsPerChannel;
                    series.remove(0, extra);
                }
            }
            hasFrame = true;
        }
        file.close();

        if (!hasFrame) {
            return false;
        }
        for (int ch = 0; ch < dsa::kChannelCount; ++ch) {
            if (!channels[ch].isEmpty()) {
                m_dataStore->appendChannelData(ch, channels[ch]);
            }
        }
        return true;
    };

    if (loadFromSessionBinary()) {
        m_acquisitionPage->refreshWaveforms(2400);
        return true;
    }

    for (int channel = 0; channel < dsa::kChannelCount; ++channel) {
        const QString filePath = dataDir.filePath(QString("ch%1.bin").arg(channel + 1));
        QFile file(filePath);
        if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const qint64 totalBytes = file.size();
        const qint64 sampleCount = totalBytes / static_cast<qint64>(sizeof(double));
        if (sampleCount <= 0) {
            file.close();
            continue;
        }

        const qint64 readCount = qMin<qint64>(sampleCount, maxPointsPerChannel);
        const qint64 readBytes = readCount * static_cast<qint64>(sizeof(double));
        file.seek(totalBytes - readBytes);
        const QByteArray raw = file.read(readBytes);
        file.close();

        if (raw.size() != readBytes) {
            continue;
        }

        QVector<double> samples(static_cast<int>(readCount));
        memcpy(samples.data(), raw.constData(), static_cast<size_t>(readBytes));
        m_dataStore->appendChannelData(channel, samples);
        loaded = true;
    }

    if (loaded) {
        m_acquisitionPage->refreshWaveforms(2400);
    }
    return loaded;
}

QString MainWindow::resolveAcquisitionDataDirectory(QString* error) const {
    if (m_projectManager != nullptr && m_projectManager->hasCurrentProject()) {
        return m_projectManager->currentProject().dataPath;
    }

    QString baseDir = QDir::homePath() + "/Desktop";
    if (!QDir(baseDir).exists()) {
        baseDir = QDir::homePath();
    }

    QDir root(baseDir);
    const QString autoRootName = "DSA_AutoData";
    if (!root.exists(autoRootName) && !root.mkpath(autoRootName)) {
        if (error) {
            *error = QString("创建自动存盘目录失败：%1").arg(root.filePath(autoRootName));
        }
        return QString();
    }

    QDir autoRoot(root.filePath(autoRootName));
    const QString dayDirName = QDate::currentDate().toString("yyyyMMdd");
    if (!autoRoot.exists(dayDirName) && !autoRoot.mkpath(dayDirName)) {
        if (error) {
            *error = QString("创建自动存盘日期目录失败：%1").arg(autoRoot.filePath(dayDirName));
        }
        return QString();
    }

    return QFileInfo(autoRoot.filePath(dayDirName)).absoluteFilePath();
}

void MainWindow::addRecentProject(const QString& projectFilePath) {
    if (projectFilePath.trimmed().isEmpty()) {
        return;
    }
    QStringList list = recentProjects();
    list.removeAll(projectFilePath);
    list.prepend(projectFilePath);
    while (list.size() > 8) {
        list.removeLast();
    }
    m_appSettings.setValue("recentProjects", list);
}

QStringList MainWindow::recentProjects() const {
    return m_appSettings.value("recentProjects").toStringList();
}

void MainWindow::refreshHomePageProject() {
    if (!m_projectManager->hasCurrentProject()) {
        m_homePage->setProjectInfo(nullptr);
        return;
    }
    const ProjectFileModel& project = m_projectManager->currentProject();
    m_homePage->setProjectInfo(&project);
}

void MainWindow::handleNetworkStateChanged(bool enabled, bool connected, const QString& message) {
    if (!m_manualNetworkConnectPending) {
        return;
    }

    if (!enabled) {
        m_manualNetworkConnectPending = false;
        m_settingsPage->setStatusMessage("网络传输未启用。", true);
        statusBar()->showMessage("网络传输未启用。", 5000);
        return;
    }

    if (connected) {
        m_manualNetworkConnectPending = false;
        const QString text = message.trimmed().isEmpty() ? QString("网络连接成功。") : message;
        m_settingsPage->setStatusMessage(text);
        statusBar()->showMessage(text, 5000);
        QMessageBox::information(this, "网络连接", text);
        return;
    }

    if (message.contains(QStringLiteral("正在连接")) || message == QStringLiteral("已更新")) {
        m_settingsPage->setStatusMessage("正在连接网络目标...");
        statusBar()->showMessage("正在连接网络目标...", 3000);
        return;
    }

    const QString text = message.trimmed().isEmpty() ? QString("网络连接失败。") : message;
    m_manualNetworkConnectPending = false;
    m_settingsPage->setStatusMessage(text, true);
    statusBar()->showMessage(text, 6000);
    showError("网络连接失败", text);
}

void MainWindow::updateSdkUi(bool ready, const QString& message) {
    m_homePage->setSdkStatus(ready, message);
    statusBar()->showMessage(message, 5000);
}

void MainWindow::updateDeviceUi(bool opened) {
    const bool receiverRunning = m_receiverService != nullptr && m_receiverService->isRunning();
    const unsigned int mode = receiverRunning ? static_cast<unsigned int>(dsa::AppMode::Receiver) : currentAppMode();
    if (m_openDeviceButton != nullptr) {
        m_openDeviceButton->setEnabled(mode != static_cast<unsigned int>(dsa::AppMode::Receiver));
    }
    if (m_acquisitionPage != nullptr) {
        m_acquisitionPage->setAppMode(mode);
    }
    m_homePage->setDeviceStatus(opened);
    m_acquisitionPage->setDeviceOpened(opened);
}

void MainWindow::updateAcquisitionUi(bool running) {
    const bool receiverRunning = m_receiverService != nullptr && m_receiverService->isRunning();
    const bool senderRunning = m_acquisitionService != nullptr && m_acquisitionService->isRunning();
    const unsigned int mode =
        receiverRunning ? static_cast<unsigned int>(dsa::AppMode::Receiver)
                        : (senderRunning ? static_cast<unsigned int>(dsa::AppMode::Sender) : currentAppMode());
    const bool isReceiverMode = mode == static_cast<unsigned int>(dsa::AppMode::Receiver);
    if (m_acquisitionPage != nullptr) {
        m_acquisitionPage->setAppMode(mode);
    }
    m_toggleAcqButton->setText(running ? (isReceiverMode ? "停止接收" : "关闭采集")
                                       : (isReceiverMode ? "开始接收" : "开始采集"));
    m_acquisitionPage->setAcquisitionRunning(running);
}

unsigned int MainWindow::currentAppMode() const {
    if (m_settingsPage == nullptr) {
        return static_cast<unsigned int>(dsa::AppMode::Sender);
    }
    return m_settingsPage->appMode();
}

void MainWindow::onPlotTick() {
    if (m_acquisitionPage == nullptr || m_stack == nullptr) {
        return;
    }

    if (m_stack->currentWidget() != m_acquisitionPage) {
        return;
    }

    const int channelCount = qMax(1, m_acquisitionPage->selectedChannels().size());
    int maxPoints = 1200;
    if (channelCount >= 12) {
        maxPoints = 600;
    } else if (channelCount >= 8) {
        maxPoints = 800;
    }

    int refreshEveryTicks = 1;
    if (m_acquisitionService != nullptr && m_acquisitionService->isRunning()) {
        if (m_lastBufferPointCount >= (dsa::kMaxPointPerChannel * 7U) / 8U) {
            refreshEveryTicks = 4;
            maxPoints = qMin(maxPoints, 240);
        } else if (m_lastBufferPointCount >= (dsa::kMaxPointPerChannel * 3U) / 4U) {
            refreshEveryTicks = 3;
            maxPoints = qMin(maxPoints, 400);
        } else if (m_lastBufferPointCount >= dsa::kMaxPointPerChannel / 2U) {
            refreshEveryTicks = 2;
            maxPoints = qMin(maxPoints, 600);
        }
    }

    ++m_plotTickCounter;
    if (refreshEveryTicks > 1 && (m_plotTickCounter % refreshEveryTicks) != 0) {
        return;
    }

    m_acquisitionPage->refreshWaveforms(maxPoints);
}

void MainWindow::onBufferPointCountUpdated(unsigned int pointsPerChannel) {
    m_lastBufferPointCount = pointsPerChannel;
    if (m_acquisitionPage != nullptr) {
        m_acquisitionPage->setBufferPointCount(pointsPerChannel);
    }
}

void MainWindow::updatePlotUi(bool running) {
    m_togglePlotButton->setText(running ? "停止绘图" : "开始绘图");
    m_acquisitionPage->setPlottingRunning(running);
    if (!running) {
        m_plotTickCounter = 0;
        m_acquisitionPage->clearWaveforms();
    }
}

void MainWindow::showError(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
    statusBar()->showMessage(message, 6000);
}

