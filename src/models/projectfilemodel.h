#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

struct ProjectFileModel {
    QString projectName;
    QString projectRoot;
    QString projectFilePath;
    QDateTime createdAt;
    QDateTime lastOpenedAt;
    QString dataPath;
    QString configPath;
    QString exportPath;
    QString cachePath;
    QJsonObject acquisitionSettings;
    QJsonObject deviceSettings;

    bool isValid(QString* error) const;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& json,
                         const QString& projectFilePath,
                         ProjectFileModel& outModel,
                         QString* error);
};

Q_DECLARE_METATYPE(ProjectFileModel)
