#pragma once

#include <QPushButton>

class MenuButton : public QPushButton {
    Q_OBJECT

public:
    explicit MenuButton(const QString& text, QWidget* parent = nullptr);

    void setLeftIconPadding(int px);
    void setRightTextPadding(int px);
    void setMiddleGap(int px);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_leftIconPadding = 10;
    int m_rightTextPadding = 12;
    int m_middleGap = 24;
};
