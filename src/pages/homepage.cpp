#include "pages/homepage.h"

#include "theme/themehelper.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QVBoxLayout>

HomePage::HomePage(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(14);

    QFrame* summaryCard = new QFrame(this);
    summaryCard->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(summaryCard);
    QVBoxLayout* summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(18, 16, 18, 16);
    summaryLayout->setSpacing(8);

    QLabel* titleLabel = new QLabel("项目信息", summaryCard);
    titleLabel->setObjectName("CardTitle");
    summaryLayout->addWidget(titleLabel);

    m_projectNameLabel = new QLabel("当前项目：未打开", summaryCard);
    m_projectPathLabel = new QLabel("项目路径：-", summaryCard);
    m_sdkStatusLabel = new QLabel("SDK 状态：未就绪", summaryCard);
    m_deviceStatusLabel = new QLabel("设备状态：未打开", summaryCard);

    QGridLayout* summaryGrid = new QGridLayout();
    summaryGrid->setContentsMargins(0, 0, 0, 0);
    summaryGrid->setHorizontalSpacing(10);
    summaryGrid->setVerticalSpacing(8);
    summaryGrid->setColumnStretch(0, 1);
    summaryGrid->setColumnStretch(1, 1);

    QLabel* infoLabels[] = {
        m_projectNameLabel,
        m_projectPathLabel,
        m_sdkStatusLabel,
        m_deviceStatusLabel};
    for (QLabel* label : infoLabels) {
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    summaryGrid->addWidget(m_projectNameLabel, 0, 0);
    summaryGrid->addWidget(m_projectPathLabel, 0, 1);
    summaryGrid->addWidget(m_sdkStatusLabel, 1, 0);
    summaryGrid->addWidget(m_deviceStatusLabel, 1, 1);
    summaryLayout->addLayout(summaryGrid);
    rootLayout->addWidget(summaryCard);

    QFrame* recentCard = new QFrame(this);
    recentCard->setObjectName("CardWidget");
    ThemeHelper::applyCardShadow(recentCard);
    QVBoxLayout* recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(18, 16, 18, 16);
    recentLayout->setSpacing(8);

    QLabel* recentTitle = new QLabel("最近项目", recentCard);
    recentTitle->setObjectName("CardTitle");
    recentLayout->addWidget(recentTitle);

    m_recentList = new QListWidget(recentCard);
    m_recentList->setAlternatingRowColors(true);
    m_recentList->setSelectionMode(QAbstractItemView::NoSelection);
    recentLayout->addWidget(m_recentList, 1);

    rootLayout->addWidget(recentCard, 1);
}

void HomePage::setProjectInfo(const ProjectFileModel* project) {
    if (project == nullptr) {
        m_projectNameLabel->setText("当前项目：未打开");
        m_projectPathLabel->setText("项目路径：-");
        return;
    }
    m_projectNameLabel->setText(QString("当前项目：%1").arg(project->projectName));
    m_projectPathLabel->setText(QString("项目路径：%1").arg(project->projectRoot));
}

void HomePage::setSdkStatus(bool ready, const QString& message) {
    m_sdkStatusLabel->setText(QString("SDK 状态：%1").arg(ready ? "已就绪" : "未就绪"));
    m_sdkStatusLabel->setToolTip(message);
}

void HomePage::setDeviceStatus(bool opened) {
    m_deviceStatusLabel->setText(QString("设备状态：%1").arg(opened ? "已打开" : "未打开"));
}

void HomePage::setRecentProjects(const QStringList& recentProjects) {
    m_recentList->clear();
    for (const QString& path : recentProjects) {
        m_recentList->addItem(path);
    }
}
