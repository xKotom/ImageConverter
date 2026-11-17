#include "SegmentedControl.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace uic {

SegmentedControl::SegmentedControl(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void SegmentedControl::addItem(const QString& text, const QVariant& data) {
    texts_ << text;
    data_ << data;
    if (index_ < 0) index_ = 0;
    updateGeometry();
    update();
}

void SegmentedControl::setCurrentIndex(int i) {
    if (i < 0 || i >= texts_.size() || i == index_) return;
    index_ = i;
    update();
    emit currentIndexChanged(index_);
}

void SegmentedControl::setCurrentData(const QVariant& d) {
    for (int i = 0; i < data_.size(); ++i)
        if (data_[i] == d) { setCurrentIndex(i); return; }
}

QSize SegmentedControl::sizeHint() const {
    const QFontMetrics fm(Theme::instance().font(13, QFont::Medium));
    int w = 0;
    for (const auto& t : texts_) w += std::max(64, fm.horizontalAdvance(t) + 28);
    return {w + 4, 32};
}

int SegmentedControl::indexAt(const QPoint& p) const {
    if (texts_.isEmpty()) return -1;
    const double segW = (width() - 4.0) / texts_.size();
    const int i = static_cast<int>((p.x() - 2) / segW);
    return (i >= 0 && i < texts_.size()) ? i : -1;
}

void SegmentedControl::paintEvent(QPaintEvent*) {
    const ThemeColors& c = Theme::instance().colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF all = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(c.fill);
    p.drawRoundedRect(all, 9, 9);
    if (texts_.isEmpty()) return;

    const double segW = (width() - 4.0) / texts_.size();
    const QFont f = Theme::instance().font(13, QFont::Medium);
    for (int i = 0; i < texts_.size(); ++i) {
        const QRectF seg(2 + i * segW, 2, segW, height() - 4.0);
        if (i == index_) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, c.dark ? 60 : 22)); // мягкая тень «таблетки»
            p.drawRoundedRect(seg.adjusted(0.5, 1.2, -0.5, 1.2), 7, 7);
            p.setBrush(c.dark ? QColor("#636366") : QColor("#FFFFFF"));
            p.drawRoundedRect(seg.adjusted(0.5, 0, -0.5, 0), 7, 7);
        } else if (i == hover_) {
            p.setPen(Qt::NoPen);
            p.setBrush(withAlpha(c.text, 0.06));
            p.drawRoundedRect(seg.adjusted(0.5, 0, -0.5, 0), 7, 7);
        }
        p.setFont(f);
        p.setPen(isEnabled() ? c.text : c.text3);
        p.drawText(seg, Qt::AlignCenter, texts_[i]);
    }
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(withAlpha(c.accent, 0.55), 2));
        p.drawRoundedRect(all, 9, 9);
    }
}

void SegmentedControl::mousePressEvent(QMouseEvent* e) {
    const int i = indexAt(e->position().toPoint());
    if (i >= 0) setCurrentIndex(i);
}

void SegmentedControl::mouseMoveEvent(QMouseEvent* e) {
    const int i = indexAt(e->position().toPoint());
    if (i != hover_) { hover_ = i; update(); }
}

void SegmentedControl::leaveEvent(QEvent*) { hover_ = -1; update(); }

void SegmentedControl::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Left) setCurrentIndex(index_ - 1);
    else if (e->key() == Qt::Key_Right) setCurrentIndex(index_ + 1);
    else QWidget::keyPressEvent(e);
}

} // namespace uic
