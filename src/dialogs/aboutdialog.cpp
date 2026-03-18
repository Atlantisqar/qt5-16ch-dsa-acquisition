#include "dialogs/aboutdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSvgWidget>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("软件信息");
    resize(420, 260);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);

    QSvgWidget* logo = new QSvgWidget(":/icons/logo.svg", this);
    logo->setFixedSize(68, 68);
    layout->addWidget(logo, 0, Qt::AlignHCenter);

    QLabel* info = new QLabel(this);
    info->setAlignment(Qt::AlignCenter);
    info->setText(
        "软件名称：16 通道动态信号采集与波形绘图软件\n"
        "版本号：v1.0.0\n"
        "公司/团队：Atom Industrial Data Team\n"
        "说明：面向工业电压采集场景，支持真实 16 通道板卡 SDK 接入、\n"
        "实时采集、动态绘图、项目管理与参数配置。");
    layout->addWidget(info);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    box->button(QDialogButtonBox::Ok)->setText("关闭");
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(box);
}
