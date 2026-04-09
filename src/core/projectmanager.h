#pragma once

#include "models/projectfilemodel.h"

#include <QObject>

class ProjectManager : public QObject {
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = nullptr);

    bool createProject(const QString& projectName, const QString& baseDir, QString* error);
    bool openProject(const QString& projectFilePath, QString* error);
    bool saveProject(QString* error);

    bool updateAcquisitionSettings(const QJsonObject& acquisitionSettings, QString* error);
    bool updateNetworkSettings(const QJsonObject& networkSettings, QString* error);
    bool updateReceiverSettings(const QJsonObject& receiverSettings, QString* error);
    bool updateDeviceSettings(const QJsonObject& deviceSettings, QString* error);
    void clearCurrentProject();

    bool hasCurrentProject() const;
    const ProjectFileModel& currentProject() const;
    QString currentProjectFilePath() const;

signals:
    void projectOpened(const ProjectFileModel& project);
    void projectSaved(const QString& projectFilePath);

private:
    bool writeProjectFile(const ProjectFileModel& model, QString* error);

private:
    ProjectFileModel m_currentProject;
    bool m_hasCurrentProject = false;
};
