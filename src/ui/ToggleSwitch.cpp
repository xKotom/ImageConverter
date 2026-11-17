#include "ToggleSwitch.h"
#include "Theme.h"

#include <QEasingCurve>
#include <QPainter>

namespace uic {

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    anim_.setDuration(160);
    anim_.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { pos_ = v.toReal(); update(); });
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        anim_.stop();
        anim_.setStartValue(pos_);
        anim_.setEndValue(on ? 1.0 : 0.0);
        anim_.start();
    });
}

void ToggleSwitch::nextCheckState() { setChecked(!isChecked()); }

void ToggleSwitch::paintEvent(QPaintEvent*) {
    const ThemeColors& c = Theme::instance().colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
    const QColor on = c.dark ? QColor("#30D158") : QColor("#34C759");
    QColor track = c.dark ? QColor("#48484A") : QColor("#D1D1D6");
    // плавное смешение цвета дорожки
    QColor mix(int(track.red() + (on.red() - track.red()) * pos_), int(track.green() + (on.green() - track.green()) * pos_),
               int(track.blue() + (on.blue() - track.blue()) * pos_));
    if (!isEnabled()) mix.setAlphaF(0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(mix);
    p.drawRoundedRect(r, r.height() / 2, r.height() / 2);

    const qreal d = r.height() - 4;
    const qreal x = r.left() + 2 + (r.width() - d - 4) * pos_;
    p.setBrush(QColor(0, 0, 0, 40)); // тень бегунка
    p.drawEllipse(QRectF(x, r.top() + 2.8, d, d));
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(x, r.top() + 2, d, d));
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(withAlpha(c.accent, 0.55), 2));
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), height() / 2.0, height() / 2.0);
    }
}

} // namespace uic
