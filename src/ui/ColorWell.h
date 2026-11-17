#pragma once
#include <QColor>
#include <QPushButton>

namespace uic {

// Круглая цветовая «капля» (как color well в macOS): клик открывает выбор цвета.
class ColorWell : public QPushButton {
    Q_OBJECT
public:
    explicit ColorWell(QWidget* parent = nullptr);
    QColor color() const { return color_; }
    void setColor(const QColor& c);
    QSize sizeHint() const override { return {30, 30}; }

signals:
    void colorChanged(const QColor& c);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QColor color_{Qt::white};
};

} // namespace uic
