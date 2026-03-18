#include "core/projectmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent) {}

bool ProjectManager::createProject(const QString& projectName, const QString& baseDir, QString* error) {
    const QString normalizedName = projectName.trimmed();
    if (normalizedName.isEmpty()) {
        if (error) {
            *error = "项目名称不能为空。";
        }
        return false;
    }

    QDir base(baseDir);
    if (!base.exists()) {
        if (error) {
            *error = QString("保存目录不存在：%1").arg(baseDir);
        }
        return false;
    }

    const QString projectRoot = base.filePath(normalizedName);
    QDir rootDir(projectRoot);
    if (!rootDir.exists() && !base.mkpath(normalizedName)) {
        if (error) {
            *error = QString("创建项目目录失败：%1").arg(projectRoot);
        }
        return false;
    }

    const QString dataPath = rootDir.filePath("data");
    const QString configPath = rootDir.filePath("config");
    const QString exportPath = rootDir.filePath("export");
    const QString cachePath = rootDir.filePath("cache");
    if (!rootDir.mkpath("data") || !rootDir.mkpath("config") || !rootDir.mkpath("export") || !rootDir.mkpath("cache")) {
        if (error) {
            *error = QString("创建项目子目录失败：%1").arg(projectRoot);
        }
        return false;
    }

    ProjectFileModel model;
    model.projectName = normalizedName;
    model.projectRoot = QFileInfo(projectRoot).absoluteFilePath();
    model.projectFilePath = rootDir.filePath(QString("%1.acqproj").arg(normalizedName));
    model.createdAt = QDateTime::currentDateTime();
    model.lastOpenedAt = model.createdAt;
    model.dataPath = QFileInfo(dataPath).absoluteFilePath();
    model.configPath = QFileInfo(configPath).absoluteFilePath();
    model.exportPath = QFileInfo(exportPath).absoluteFilePath();
    model.cachePath = QFileInfo(cachePath).absoluteFilePath();

    if (!writeProjectFile(model, error)) {
        return false;
    }

    m_currentProject = model;
    m_hasCurrentProject = true;
    emit projectOpened(m_currentProject);
    emit projectSaved(m_currentProject.projectFilePath);
    return true;
}

bool ProjectManager::openProject(const QString& projectFilePath, QString* error) {
    QFile file(projectFilePath);
    if (!file.exists()) {
        if (error) {
            *error = QString("项目文件不存在：%1").arg(projectFilePath);
        }
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QString("项目文件打开失败：%1").arg(projectFilePath);
        }
        return false;
    }

    const QByteArray raw = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QString("项目文件损坏或格式无效：%1").arg(parseError.errorString());
        }
        return false;
    }

    ProjectFileModel model;
    QString parseModelError;
    if (!ProjectFileModel::fromJson(document.object(), QFileInfo(projectFilePath).absoluteFilePath(), model, &parseModelError)) {
        if (error) {
            *error = parseModelError;
        }
        return false;
    }

    QDir modelRoot(model.projectRoot);
    if (!modelRoot.exists()) {
        if (error) {
            *error = QString("项目根目录不存在：%1").arg(model.projectRoot);
        }
        return false;
    }
    modelRoot.mkpath("data");
    modelRoot.mkpath("config");
    modelRoot.mkpath("export");
    modelRoot.mkpath("cache");

    model.lastOpenedAt = QDateTime::currentDateTime();
    if (!writeProjectFile(model, error)) {
        return false;
    }

    m_currentProject = model;
    m_hasCurrentProject = true;
    emit projectOpened(m_currentProject);
    return true;
}

bool ProjectManager::saveProject(QString* error) {
    if (!m_hasCurrentProject) {
        if (error) {
            *error = "当前没有已打开项目。";
        }
        return false;
    }
    m_currentProject.lastOpenedAt = QDateTime::currentDateTime();
    if (!writeProjectFile(m_currentProject, error)) {
        return false;
    }
    emit projectSaved(m_currentProject.projectFilePath);
    return true;
}

bool ProjectManager::updateAcquisitionSettings(const QJsonObject& acquisitionSettings, QString* error) {
    if (!m_hasCurrentProject) {
        if (error) {
            *error = "当前没有项目，无法保存采集参数。";
        }
        return false;
    }
    m_currentProject.acquisitionSettings = acquisitionSettings;
    return saveProject(error);
}

bool ProjectManager::updateDeviceSettings(const QJsonObject& deviceSettings, QString* error) {
    if (!m_hasCurrentProject) {
        if (error) {
            *error = "当前没有项目，无法保存设备参数。";
        }
        return false;
    }
    m_currentProject.deviceSettings = deviceSettings;
    return saveProject(error);
}

void ProjectManager::clearCurrentProject() {
    m_currentProject = ProjectFileModel();
    m_hasCurrentProject = false;
}

bool ProjectManager::hasCurrentProject() const {
    return m_hasCurrentProject;
}

const ProjectFileModel& ProjectManager::currentProject() const {
    return m_currentProject;
}

QString ProjectManager::currentProjectFilePath() const {
    return m_hasCurrentProject ? m_currentProject.projectFilePath : QString();
}

bool ProjectManager::writeProjectFile(const ProjectFileModel& model, QString* error) {
    QString modelError;
    if (!model.isValid(&modelError)) {
        if (error) {
            *error = modelError;
        }
        return false;
    }

    QFile file(model.projectFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QString("写入项目文件失败：%1").arg(model.projectFilePath);
        }
        return false;
    }
    const QJsonDocument document(model.toJson());
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        if (error) {
            *error = QString("写入项目文件失败：%1").arg(model.projectFilePath);
        }
        file.close();
        return false;
    }
    file.close();
    return true;
}
