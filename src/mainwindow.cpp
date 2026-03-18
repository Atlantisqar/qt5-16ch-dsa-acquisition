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
    m_plotService = new PlotService(this);

    if (qEnvironmentVariableIsSet("DSA_USE_MOCK")) {
        m_acquisitionService->setMockModeEnabled(true);
    }

    buildUi();
    bindSignals();
    initializeSdk();
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
    if (m_acquisitionService->isRunning()) {
        m_acquisitionService->stopAcquisition();
        return;
    }

    dsa::DsaAcquisitionSettings settings;
    QString validationError;
    if (!m_settingsPage->collectSettings(settings, &validationError)) {
        showError("参数错误", validationError);
        return;
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
    QString dataDirError;
    const QString dataDir = resolveAcquisitionDataDirectory(&dataDirError);
    if (dataDir.isEmpty()) {
        showError("开始采集失败", dataDirError);
        return;
    }
    m_acquisitionService->setDataDirectory(dataDir);

    if (!m_acquisitionService->startAcquisition()) {
        showError("开始采集失败", m_acquisitionService->lastError());
        return;
    }
    if (!m_projectManager->hasCurrentProject()) {
        statusBar()->showMessage(QString("未打开项目，数据将保存到：%1").arg(dataDir), 6000);
    }
}

void MainWindow::onTogglePlotClicked() {
    if (m_plotService->isRunning()) {
        m_plotService->stopPlotting();
        return;
    }

    if (!m_acquisitionService->isRunning()) {
        const bool loaded = loadHistoryDataToStore(4096);
        if (!loaded) {
            statusBar()->showMessage("未检测到历史数据，绘图将等待实时数据。", 5000);
        }
    }

    m_plotService->startPlotting(16);
}

void MainWindow::onQuickStartClicked() {
    if (m_acquisitionService->isRunning()) {
        m_stack->setCurrentWidget(m_acquisitionPage);
        statusBar()->showMessage("采集已在运行。", 3000);
        return;
    }

    const dsa::DsaAcquisitionSettings defaultSettings{};
    QString validationError;
    if (!defaultSettings.validate(&validationError)) {
        showError("一键启动失败", QString("默认参数无效：%1").arg(validationError));
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
    m_acquisitionService->setAcquisitionSettings(defaultSettings);
    m_settingsPage->setStatusMessage("已应用默认参数并启动采集流程。");

    QString dataDirError;
    const QString dataDir = resolveAcquisitionDataDirectory(&dataDirError);
    if (dataDir.isEmpty()) {
        showError("一键启动失败", dataDirError);
        return;
    }
    m_acquisitionService->setDataDirectory(dataDir);

    if (!m_acquisitionService->startAcquisition()) {
        showError("一键启动失败", QString("启动采集失败：%1").arg(m_acquisitionService->lastError()));
        return;
    }

    m_stack->setCurrentWidget(m_acquisitionPage);
    if (hasProject) {
        statusBar()->showMessage("一键启动完成：设备已打开，默认参数已应用，采集已开始。", 4000);
    } else {
        statusBar()->showMessage(QString("一键启动完成：已开始采集，数据保存到：%1").arg(dataDir), 6000);
    }
}

void MainWindow::onQuickClearClicked() {
    m_plotService->stopPlotting();
    m_acquisitionService->stopAcquisition();
    m_deviceService->shutdown();
    m_deviceService->resetCachedState();
    initializeSdk();

    m_dataStore->clear();
    m_acquisitionService->setDataDirectory(QString());

    const dsa::DsaAcquisitionSettings defaultSettings{};
    m_settingsPage->setSettings(defaultSettings);
    m_settingsPage->setDioInputValue(0x00);
    m_settingsPage->setStatusMessage("已恢复默认状态。");
    m_acquisitionService->setAcquisitionSettings(defaultSettings);

    m_acquisitionPage->setSelectedChannels(QVector<int>{0, 1, 2, 3});
    m_acquisitionPage->setAcquisitionRunning(false);
    m_acquisitionPage->setPlottingRunning(false);
    m_acquisitionPage->setOverflow(false);
    m_acquisitionPage->setBufferPointCount(0);
    m_acquisitionPage->refreshWaveforms(2400);

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
    m_settingsPage->setSettings(m_deviceService->currentSettings());
    m_settingsPage->setStatusMessage("已加载当前缓存配置。");
}

void MainWindow::onApplySettingsRequested(const dsa::DsaAcquisitionSettings& settings) {
    if (!m_deviceService->isDeviceOpen()) {
        m_acquisitionService->setAcquisitionSettings(settings);
        m_settingsPage->setStatusMessage("设备未打开，已保存为本地待应用参数。");
        return;
    }
    if (!m_deviceService->setAcquisitionSettings(settings)) {
        m_settingsPage->setStatusMessage(m_deviceService->lastError(), true);
        showError("应用参数失败", m_deviceService->lastError());
        return;
    }
    m_acquisitionService->setAcquisitionSettings(settings);
    m_settingsPage->setStatusMessage("参数已成功下发到设备。");
}

void MainWindow::onSaveToProjectRequested(const dsa::DsaAcquisitionSettings& settings,
                                          const dsa::DsaDioSettings& dioSettings) {
    if (!m_projectManager->hasCurrentProject()) {
        showError("保存失败", "当前没有打开的项目。");
        return;
    }

    QString error;
    if (!m_projectManager->updateAcquisitionSettings(settings.toJson(), &error)) {
        showError("保存失败", error);
        return;
    }
    if (!m_projectManager->updateDeviceSettings(dioSettings.toJson(), &error)) {
        showError("保存失败", error);
        return;
    }
    m_settingsPage->setStatusMessage("参数已保存到项目文件。");
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
    sideMenu->setFixedWidth(176);
    ThemeHelper::applyCardShadow(sideMenu);
    QVBoxLayout* sideLayout = new QVBoxLayout(sideMenu);
    sideLayout->setContentsMargins(16, 16, 16, 16);
    sideLayout->setSpacing(10);

    QLabel* appTitle = new QLabel("16CH DSA", sideMenu);
    appTitle->setObjectName("AppTitle");
    appTitle->setAlignment(Qt::AlignCenter);
    QLabel* appSubTitle = new QLabel(sideMenu);
    appSubTitle->setObjectName("AppSubTitle");
    const QPixmap subTitleImage(":/brand/cumtb2.png");
    if (!subTitleImage.isNull()) {
        appSubTitle->setPixmap(subTitleImage.scaled(140, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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

    QWidget* topStatusCard = new QWidget(rightArea);
    topStatusCard->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(topStatusCard);
    QHBoxLayout* statusLayout = new QHBoxLayout(topStatusCard);
    statusLayout->setContentsMargins(16, 14, 16, 14);
    statusLayout->setSpacing(16);

    m_statusProjectLabel = new QLabel("项目：未打开", topStatusCard);
    m_statusSdkLabel = new QLabel("SDK：未就绪", topStatusCard);
    m_statusDeviceLabel = new QLabel("设备：未打开", topStatusCard);
    m_statusAcqLabel = new QLabel("采集：已停止", topStatusCard);
    m_statusPlotLabel = new QLabel("绘图：已停止", topStatusCard);
    QLabel* topStatusLabels[] = {
        m_statusProjectLabel,
        m_statusSdkLabel,
        m_statusDeviceLabel,
        m_statusAcqLabel,
        m_statusPlotLabel};
    for (QLabel* label : topStatusLabels) {
        QFont f = label->font();
        if (f.pointSizeF() > 0.0) {
            f.setPointSizeF(f.pointSizeF() * 1.265);
        } else if (f.pointSize() > 0) {
            f.setPointSizeF(static_cast<double>(f.pointSize()) * 1.265);
        } else if (f.pixelSize() > 0) {
            f.setPixelSize(qRound(static_cast<double>(f.pixelSize()) * 1.265));
        }
        label->setFont(f);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        statusLayout->addWidget(label, 1);
    }
    topStatusCard->setMinimumHeight(qRound(static_cast<double>(topStatusCard->sizeHint().height()) * 1.15));

    rightLayout->addWidget(topStatusCard);

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
    button->setLeftIconPadding(8);
    button->setRightTextPadding(12);
    button->setMiddleGap(20);
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
        }
    });

    connect(m_deviceService, &Dsa16ChDeviceService::sdkStateChanged, this, &MainWindow::updateSdkUi);
    connect(m_deviceService, &Dsa16ChDeviceService::deviceOpenStateChanged, this, &MainWindow::updateDeviceUi);

    connect(m_settingsPage, &SettingsPage::readSettingsRequested, this, &MainWindow::onReadSettingsRequested);
    connect(m_settingsPage, &SettingsPage::applySettingsRequested, this, &MainWindow::onApplySettingsRequested);
    connect(m_settingsPage, &SettingsPage::saveToProjectRequested, this, &MainWindow::onSaveToProjectRequested);
    connect(m_settingsPage, &SettingsPage::dioDirectionSetRequested, this, &MainWindow::onDioDirectionSetRequested);
    connect(m_settingsPage, &SettingsPage::dioWriteRequested, this, &MainWindow::onDioWriteRequested);
    connect(m_settingsPage, &SettingsPage::dioReadRequested, this, &MainWindow::onDioReadRequested);

    connect(m_acquisitionService, &AcquisitionService::runningChanged, this, &MainWindow::updateAcquisitionUi);
    connect(m_acquisitionService, &AcquisitionService::bufferPointCountUpdated, m_acquisitionPage, &AcquisitionPage::setBufferPointCount);
    connect(m_acquisitionService, &AcquisitionService::overflowDetected, m_acquisitionPage, &AcquisitionPage::setOverflow);
    connect(m_acquisitionService, &AcquisitionService::errorOccurred, this, [this](const QString& message) {
        statusBar()->showMessage(message, 5000);
    });

    connect(m_plotService, &PlotService::plottingChanged, this, &MainWindow::updatePlotUi);
    connect(m_plotService, &PlotService::plottingTick, this, [this]() { m_acquisitionPage->refreshWaveforms(2400); });
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
        m_statusProjectLabel->setText("项目：未打开");
        return;
    }
    const ProjectFileModel& project = m_projectManager->currentProject();
    m_homePage->setProjectInfo(&project);
    m_statusProjectLabel->setText(QString("项目：%1").arg(project.projectName));
}

void MainWindow::updateSdkUi(bool ready, const QString& message) {
    m_statusSdkLabel->setText(QString("SDK：%1").arg(ready ? "已就绪" : "未就绪"));
    m_statusSdkLabel->setToolTip(message);
    m_homePage->setSdkStatus(ready, message);
    statusBar()->showMessage(message, 5000);
}

void MainWindow::updateDeviceUi(bool opened) {
    m_statusDeviceLabel->setText(QString("设备：%1").arg(opened ? "已打开" : "未打开"));
    m_homePage->setDeviceStatus(opened);
}

void MainWindow::updateAcquisitionUi(bool running) {
    m_toggleAcqButton->setText(running ? "关闭采集" : "开始采集");
    m_statusAcqLabel->setText(QString("采集：%1").arg(running ? "运行中" : "已停止"));
    m_acquisitionPage->setAcquisitionRunning(running);
}

void MainWindow::updatePlotUi(bool running) {
    m_togglePlotButton->setText(running ? "停止绘图" : "开始绘图");
    m_statusPlotLabel->setText(QString("绘图：%1").arg(running ? "运行中" : "已停止"));
    m_acquisitionPage->setPlottingRunning(running);
}

void MainWindow::showError(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
    statusBar()->showMessage(message, 6000);
}

