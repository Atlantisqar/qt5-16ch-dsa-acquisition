#include "widgets/menubutton.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>

MenuButton::MenuButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent) {}

void MenuButton::setLeftIconPadding(int px) {
    m_leftIconPadding = qMax(0, px);
    update();
}

void MenuButton::setRightTextPadding(int px) {
    m_rightTextPadding = qMax(0, px);
    update();
}

void MenuButton::setMiddleGap(int px) {
    m_middleGap = qMax(0, px);
    update();
}

void MenuButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QStyleOptionButton option;
    initStyleOption(&option);

    QPainter painter(this);
    style()->drawControl(QStyle::CE_PushButtonBevel, &option, &painter, this);

    const QSize iconSz = iconSize().isValid() ? iconSize() : QSize(16, 16);
    const int iconY = (height() - iconSz.height()) / 2;
    const QRect iconRect(m_leftIconPadding, iconY, iconSz.width(), iconSz.height());

    const QRect textRect(iconRect.right() + 1 + m_middleGap,
                         0,
                         width() - (iconRect.right() + 1 + m_middleGap) - m_rightTextPadding,
                         height());

    QIcon::Mode mode = QIcon::Normal;
    if (!(option.state & QStyle::State_Enabled)) {
        mode = QIcon::Disabled;
    } else if (option.state & QStyle::State_MouseOver) {
        mode = QIcon::Active;
    }
    const QPixmap pix = icon().pixmap(iconSz, mode, QIcon::Off);
    if (!pix.isNull()) {
        painter.drawPixmap(iconRect, pix);
    }

    painter.setFont(font());
    painter.setPen(palette().color(QPalette::ButtonText));
    painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, text());

    if (hasFocus()) {
        QStyleOptionFocusRect focusOpt;
        focusOpt.initFrom(this);
        focusOpt.backgroundColor = palette().color(QPalette::Button);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOpt, &painter, this);
    }
}
