#include "ColorWell.h"
#include "Theme.h"

#include <QColorDialog>
#include <QPainter>

namespace uic {

ColorWell::ColorWell(QWidget* parent) : QPushButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(30, 30);
    setFlat(true);
    connect(this, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(color_, window(), "Фон вместо прозрачности");
        if (c.isValid()) { setColor(c); emit colorChanged(c); }
    });
}

void ColorWell::setColor(const QColor& c) {
    color_ = c;
    update();
}

void ColorWell::paintEvent(QPaintEvent*) {
    const ThemeColors& tc = Theme::instance().colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
    p.setPen(QPen(withAlpha(tc.text, 0.25), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r);                       // внешнее кольцо
    p.setPen(Qt::NoPen);
    p.setBrush(color_);
    p.drawEllipse(r.adjusted(3, 3, -3, -3)); // сама «капля»
    if (isEnabled() == false) {
        p.setBrush(withAlpha(tc.card, 0.5));
        p.drawEllipse(r);
    }
}

} // namespace uic
