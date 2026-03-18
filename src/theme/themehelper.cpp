#include "theme/themehelper.h"

#include <QGraphicsDropShadowEffect>

namespace ThemeHelper {

void applyCardShadow(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    auto* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(36.0);
    shadow->setOffset(0.0, 6.0);
    shadow->setColor(QColor(20, 38, 64, 58));
    widget->setGraphicsEffect(shadow);
}

}  // namespace ThemeHelper
