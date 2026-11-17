#pragma once
#include <QStringList>
#include <QVariant>
#include <QWidget>

namespace uic {

// Сегментированный переключатель в стиле macOS/iOS: серая дорожка и «таблетка» выбранного пункта.
class SegmentedControl : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedControl(QWidget* parent = nullptr);

    void addItem(const QString& text, const QVariant& data = {});
    int currentIndex() const { return index_; }
    QVariant currentData() const { return index_ >= 0 ? data_.value(index_) : QVariant(); }
    void setCurrentIndex(int i);
    void setCurrentData(const QVariant& d);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

signals:
    void currentIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    int indexAt(const QPoint& p) const;
    QStringList texts_;
    QVariantList data_;
    int index_ = -1;
    int hover_ = -1;
};

} // namespace uic
