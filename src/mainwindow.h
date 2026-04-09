#pragma once

#include "core/acquisitionservice.h"
#include "core/acquisitiontcpreceiverservice.h"
#include "core/dsa16chdeviceservice.h"
#include "core/dsa16chdllloader.h"
#include "core/multichanneldatastore.h"
#include "core/plotservice.h"
#include "core/projectmanager.h"
#include "pages/acquisitionpage.h"
#include "pages/homepage.h"
#include "pages/settingspage.h"

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSettings>

class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& startupProjectFile = QString(), QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewProjectClicked();
    void onOpenProjectClicked();
    void onOpenDeviceClicked();
    void onShowSettingsPageClicked();
    void onShowAcquisitionPageClicked();
    void onToggleAcquisitionClicked();
    void onTogglePlotClicked();
    void onQuickStartClicked();
    void onQuickClearClicked();
    void onShowAboutClicked();

    void onReadSettingsRequested();
    void onApplySettingsRequested(const dsa::DsaAcquisitionSettings& settings,
                                  const dsa::DsaNetworkSettings& networkSettings,
                                  const dsa::DsaReceiverSettings& receiverSettings);
    void onConnectNetworkRequested(const dsa::DsaNetworkSettings& networkSettings);
    void onSaveToProjectRequested(const dsa::DsaAcquisitionSettings& settings,
                                  const dsa::DsaDioSettings& dioSettings,
                                  const dsa::DsaNetworkSettings& networkSettings,
                                  const dsa::DsaReceiverSettings& receiverSettings);
    void onDioDirectionSetRequested(unsigned int groupIndex, unsigned int direction);
    void onDioWriteRequested(unsigned int groupIndex, unsigned char doData);
    void onDioReadRequested(unsigned int groupIndex);
    void onPlotTick();
    void onBufferPointCountUpdated(unsigned int pointsPerChannel);

private:
    void buildUi();
    QPushButton* createMenuButton(const QString& text);
    void bindSignals();
    void initializeSdk();
    bool openProjectFile(const QString& projectFilePath, bool showMessage);
    bool loadHistoryDataToStore(int maxPointsPerChannel);
    QString resolveAcquisitionDataDirectory(QString* error) const;
    unsigned int currentAppMode() const;
    void addRecentProject(const QString& projectFilePath);
    QStringList recentProjects() const;
    void refreshHomePageProject();
    void handleNetworkStateChanged(bool enabled, bool connected, const QString& message);
    void updateSdkUi(bool ready, const QString& message);
    void updateDeviceUi(bool opened);
    void updateAcquisitionUi(bool running);
    void updatePlotUi(bool running);
    void showError(const QString& title, const QString& message);

private:
    ProjectManager* m_projectManager = nullptr;
    Dsa16ChDllLoader* m_dllLoader = nullptr;
    Dsa16ChDeviceService* m_deviceService = nullptr;
    MultiChannelDataStore* m_dataStore = nullptr;
    AcquisitionService* m_acquisitionService = nullptr;
    AcquisitionTcpReceiverService* m_receiverService = nullptr;
    PlotService* m_plotService = nullptr;

    QWidget* m_centralRoot = nullptr;
    QStackedWidget* m_stack = nullptr;
    HomePage* m_homePage = nullptr;
    SettingsPage* m_settingsPage = nullptr;
    AcquisitionPage* m_acquisitionPage = nullptr;
    QWidget* m_historyPlaceholderPage = nullptr;

    QPushButton* m_newProjectButton = nullptr;
    QPushButton* m_openProjectButton = nullptr;
    QPushButton* m_openDeviceButton = nullptr;
    QPushButton* m_settingsPageButton = nullptr;
    QPushButton* m_acquisitionPageButton = nullptr;
    QPushButton* m_toggleAcqButton = nullptr;
    QPushButton* m_togglePlotButton = nullptr;
    QPushButton* m_quickStartButton = nullptr;
    QPushButton* m_quickClearButton = nullptr;
    QPushButton* m_aboutButton = nullptr;

    QSettings m_appSettings;
    unsigned int m_lastBufferPointCount = 0;
    int m_plotTickCounter = 0;
    bool m_manualNetworkConnectPending = false;
};
