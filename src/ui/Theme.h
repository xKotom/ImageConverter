#pragma once
#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

class QApplication;
class QWidget;

namespace uic {

// Как выбирается светлая/тёмная схема.
enum class Appearance { System, Light, Dark };

// Палитра в духе macOS/iOS. Цвета могут быть полупрозрачными (стеклянный стиль).
struct ThemeColors {
    bool dark = false;
    bool glass = false;
    QColor bg, card, cardBorder, popup, text, text2, text3, separator, fill, fillHover;
    QColor accent, accentText, accentHover, accentSoft, danger, success, warning, track, scroll;
};

class Theme : public QObject {
    Q_OBJECT
public:
    static Theme& instance();

    const ThemeColors& colors() const { return colors_; }
    QFont font(int pixelSize = 13, QFont::Weight weight = QFont::Normal) const;

    Appearance appearance() const { return appearance_; }
    bool glass() const { return glass_; }
    // Применяют изменение сразу ко всему приложению (без перезапуска).
    void setAppearance(Appearance a);
    void setGlass(bool on);

    // Первичное применение стилей; дальше тема следит за системной схемой сама.
    void apply(QApplication& app, Appearance appearance, bool glass);

    // Доступно ли настоящее размытие фона окна (акрил Windows 10 1803+ / Windows 11).
    bool realBlur() const { return realBlur_; }

    // Фон окна в стеклянном стиле: при настоящем размытии — лёгкая вуаль и блики поверх размытого фона
    // рабочего стола; без него — цветной градиент. Для обычного стиля ничего не рисует.
    void paintBackdrop(QWidget* w) const;
    // Заголовок окна под цвет темы (Windows 11: тёмный режим, цвет и текст заголовка).
    void applyWindowChrome(QWidget* w) const;

signals:
    void changed();

private:
    Theme() = default;
    void refresh();
    void rebuild(bool dark);
    QString styleSheet() const;

    ThemeColors colors_;
    Appearance appearance_ = Appearance::System;
    bool glass_ = false;
    bool connected_ = false;
    bool realBlur_ = false;
    QApplication* app_ = nullptr;
};

// Цвет с прозрачностью (0..1) — для «мягких» подложек.
inline QColor withAlpha(QColor c, qreal a) { c.setAlphaF(a); return c; }

} // namespace uic
