#include "models/projectfilemodel.h"

#include <QDir>

bool ProjectFileModel::isValid(QString* error) const {
    if (projectName.trimmed().isEmpty()) {
        if (error) {
            *error = "项目名称为空。";
        }
        return false;
    }
    if (projectRoot.trimmed().isEmpty()) {
        if (error) {
            *error = "项目根目录为空。";
        }
        return false;
    }
    if (!QDir(projectRoot).exists()) {
        if (error) {
            *error = QString("项目根目录不存在：%1").arg(projectRoot);
        }
        return false;
    }
    if (projectFilePath.trimmed().isEmpty()) {
        if (error) {
            *error = "项目文件路径为空。";
        }
        return false;
    }
    return true;
}

QJsonObject ProjectFileModel::toJson() const {
    QJsonObject json;
    json["projectName"] = projectName;
    json["projectRoot"] = projectRoot;
    json["createdAt"] = createdAt.toString(Qt::ISODate);
    json["dataPath"] = dataPath;
    json["configPath"] = configPath;
    json["exportPath"] = exportPath;
    json["cachePath"] = cachePath;
    json["lastOpenedAt"] = lastOpenedAt.toString(Qt::ISODate);
    json["acquisitionSettings"] = acquisitionSettings;
    json["deviceSettings"] = deviceSettings;
    return json;
}

bool ProjectFileModel::fromJson(const QJsonObject& json,
                                const QString& path,
                                ProjectFileModel& outModel,
                                QString* error) {
    ProjectFileModel model;
    model.projectName = json.value("projectName").toString();
    model.projectRoot = json.value("projectRoot").toString();
    model.projectFilePath = path;
    model.createdAt = QDateTime::fromString(json.value("createdAt").toString(), Qt::ISODate);
    model.dataPath = json.value("dataPath").toString();
    model.configPath = json.value("configPath").toString();
    model.exportPath = json.value("exportPath").toString();
    model.cachePath = json.value("cachePath").toString();
    model.lastOpenedAt = QDateTime::fromString(json.value("lastOpenedAt").toString(), Qt::ISODate);
    model.acquisitionSettings = json.value("acquisitionSettings").toObject();
    model.deviceSettings = json.value("deviceSettings").toObject();

    if (!model.isValid(error)) {
        return false;
    }

    outModel = model;
    return true;
}
