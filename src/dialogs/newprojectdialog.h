#pragma once

#include <QDialog>

class QLineEdit;

class NewProjectDialog : public QDialog {
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget* parent = nullptr);

    QString projectName() const;
    QString projectBaseDirectory() const;

private slots:
    void onBrowseClicked();
    void onAcceptClicked();

private:
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
};
