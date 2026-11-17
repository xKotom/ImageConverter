#pragma once
#include <QAbstractButton>
#include <QVariantAnimation>

namespace uic {

// Переключатель в стиле iOS: зелёная «капсула» с белым бегунком и плавной анимацией.
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override { return {46, 28}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void nextCheckState() override;
    bool hitButton(const QPoint& pos) const override { return rect().contains(pos); }

private:
    QVariantAnimation anim_;
    qreal pos_ = 0; // 0..1
};

} // namespace uic
