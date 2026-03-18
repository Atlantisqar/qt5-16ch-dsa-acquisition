#include "dialogs/newprojectdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("新建项目"));
    resize(540, 180);

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);

    QFormLayout* form = new QFormLayout();
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText("Project1");
    m_nameEdit->setPlaceholderText(QStringLiteral("请输入项目名称"));

    QWidget* dirWidget = new QWidget(this);
    QHBoxLayout* dirLayout = new QHBoxLayout(dirWidget);
    dirLayout->setContentsMargins(0, 0, 0, 0);

    m_directoryEdit = new QLineEdit(dirWidget);
    QString defaultBaseDir;
    const QDir desktopDir(QDir::homePath() + "/Desktop");
    if (desktopDir.exists()) {
        defaultBaseDir = desktopDir.absolutePath();
    } else {
        defaultBaseDir = QDir::homePath();
    }
    m_directoryEdit->setText(defaultBaseDir);
    m_directoryEdit->setPlaceholderText(QStringLiteral("请选择项目保存目录"));

    QPushButton* browseButton = new QPushButton(QStringLiteral("浏览"), dirWidget);
    connect(browseButton, &QPushButton::clicked, this, &NewProjectDialog::onBrowseClicked);
    dirLayout->addWidget(m_directoryEdit, 1);
    dirLayout->addWidget(browseButton);

    form->addRow(QStringLiteral("项目名称"), m_nameEdit);
    form->addRow(QStringLiteral("保存目录"), dirWidget);
    rootLayout->addLayout(form);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("创建"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(box->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &NewProjectDialog::onAcceptClicked);
    connect(box->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &NewProjectDialog::reject);
    rootLayout->addWidget(box);
}

QString NewProjectDialog::projectName() const {
    return m_nameEdit->text().trimmed();
}

QString NewProjectDialog::projectBaseDirectory() const {
    return m_directoryEdit->text().trimmed();
}

void NewProjectDialog::onBrowseClicked() {
    const QString dir = QFileDialog::getExistingDirectory(this,
                                                          QStringLiteral("选择项目保存目录"),
                                                          projectBaseDirectory());
    if (!dir.isEmpty()) {
        m_directoryEdit->setText(dir);
    }
}

void NewProjectDialog::onAcceptClicked() {
    if (projectName().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("项目名称不能为空。"));
        return;
    }
    if (projectBaseDirectory().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数错误"), QStringLiteral("请先选择项目保存目录。"));
        return;
    }
    accept();
}
