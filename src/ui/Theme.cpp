#include "Theme.h"
#include "Icons.h"

#include <QApplication>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QStyleHints>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#endif

namespace uic {

#ifdef Q_OS_WIN
namespace {

DWORD windowsBuild() {
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    static const DWORD build = [] {
        OSVERSIONINFOW vi{};
        vi.dwOSVersionInfoSize = sizeof vi;
        if (HMODULE nt = GetModuleHandleW(L"ntdll.dll"))
            if (auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(nt, "RtlGetVersion")))
                fn(&vi);
        return vi.dwBuildNumber;
    }();
    return build;
}

// Акрил через недокументированный SetWindowCompositionAttribute (Windows 10 1803+ и ранние сборки Windows 11).
void setAccentAcrylic(HWND hwnd, bool on, const QColor& tint) {
    struct AccentPolicy { int state; int flags; unsigned color; int animationId; };
    struct CompositionData { int attribute; void* data; ULONG size; };
    using Fn = BOOL(WINAPI*)(HWND, CompositionData*);
    static const Fn fn = [] {
        HMODULE u = GetModuleHandleW(L"user32.dll");
        return u ? reinterpret_cast<Fn>(GetProcAddress(u, "SetWindowCompositionAttribute")) : nullptr;
    }();
    if (!fn) return;
    AccentPolicy ap{};
    ap.state = on ? 4 /*ACCENT_ENABLE_ACRYLICBLURBEHIND*/ : 0 /*ACCENT_DISABLED*/;
    ap.flags = 2;
    // порядок байтов AABBGGRR
    ap.color = (static_cast<unsigned>(tint.alpha()) << 24) | (static_cast<unsigned>(tint.blue()) << 16) |
               (static_cast<unsigned>(tint.green()) << 8) | static_cast<unsigned>(tint.red());
    CompositionData cd{19 /*WCA_ACCENT_POLICY*/, &ap, sizeof ap};
    fn(hwnd, &cd);
}

} // namespace
#endif

Theme& Theme::instance() {
    static Theme t;
    return t;
}

QFont Theme::font(int pixelSize, QFont::Weight weight) const {
    QFont f;
    f.setFamilies({"SF Pro Text", "Segoe UI Variable Text", "Segoe UI", "Helvetica Neue", "Arial"});
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    return f;
}

namespace {
// CSS-цвет: с прозрачностью — rgba(), иначе #rrggbb.
QString css(const QColor& c) {
    if (c.alpha() == 255) return c.name();
    return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alphaF(), 0, 'f', 3);
}
} // namespace

void Theme::rebuild(bool dark) {
    ThemeColors c;
    c.dark = dark;
    c.glass = glass_;
    if (!dark) {
        c.bg = "#F5F5F7";        c.card = "#FFFFFF";
        c.text = "#1D1D1F";      c.text2 = "#6E6E73";   c.text3 = "#AEAEB2";
        c.separator = "#E5E5EA"; c.fill = "#EDEDF0";    c.fillHover = "#E2E2E7";
        c.accent = "#007AFF";    c.accentHover = "#0A6CE0";
        c.danger = "#FF3B30";    c.success = "#248A3D"; c.warning = "#E08600";
        c.track = "#E5E5EA";     c.scroll = QColor(0, 0, 0, 70);
        c.cardBorder = c.separator; c.popup = "#FFFFFF";
    } else {
        c.bg = "#1C1C1E";        c.card = "#2C2C2E";
        c.text = "#F5F5F7";      c.text2 = "#98989D";   c.text3 = "#636366";
        c.separator = "#3A3A3C"; c.fill = "#3A3A3C";    c.fillHover = "#48484A";
        c.accent = "#0A84FF";    c.accentHover = "#3D9BFF";
        c.danger = "#FF453A";    c.success = "#30D158"; c.warning = "#FF9F0A";
        c.track = "#48484A";     c.scroll = QColor(255, 255, 255, 70);
        c.cardBorder = c.separator; c.popup = "#2C2C2E";
    }
    if (glass_) {
        // «Стекло»: полупрозрачные панели поверх цветного фона, светлая кромка.
        if (!dark) {
            c.bg = "#E4ECFA";
            c.card = QColor(255, 255, 255, 120);
            c.cardBorder = QColor(255, 255, 255, 190);
            c.separator = QColor(0, 0, 0, 22);
            c.fill = QColor(0, 0, 0, 16);       c.fillHover = QColor(0, 0, 0, 28);
            c.track = QColor(0, 0, 0, 28);
            c.text2 = "#5A5A60";
        } else {
            c.bg = "#101528";
            c.card = QColor(255, 255, 255, 24);
            c.cardBorder = QColor(255, 255, 255, 46);
            c.separator = QColor(255, 255, 255, 30);
            c.fill = QColor(255, 255, 255, 30); c.fillHover = QColor(255, 255, 255, 48);
            c.track = QColor(255, 255, 255, 46);
            c.text2 = "#A6A6B0";
        }
    }
    c.accentText = dark ? QColor("#64B0FF") : c.accent;
    c.accentSoft = withAlpha(c.accent, dark ? 0.24 : 0.14);
    colors_ = c;
}

QString Theme::styleSheet() const {
    const ThemeColors& c = colors_;
    // Стеклянная карточка: плоская полупрозрачная заливка (цвет и прозрачность заданы в палитре).
    const QString cardBg = css(c.card);

    QString s = R"(
* { font-family: "SF Pro Text","Segoe UI Variable Text","Segoe UI"; font-size: 13px; color: @text@; }
QMainWindow, QDialog, QMessageBox { background: @bg@; }
QWidget#transparent, QStackedWidget, QScrollArea { background: transparent; }
QLabel { background: transparent; }
QLabel[role="title"]   { font-size: 22px; font-weight: 700; }
QLabel[role="heading"] { font-size: 15px; font-weight: 600; }
QLabel[role="row"]     { font-size: 13px; font-weight: 500; }
QLabel[role="caption"] { color: @text2@; font-size: 12px; }
QLabel[role="section"] { color: @text2@; font-size: 11px; font-weight: 600; letter-spacing: 0.5px; }
QLabel[role="preview"] { background: @fill@; border-radius: 10px; color: @text2@; }
QFrame[card="true"] { background: @cardBg@; border: 1px solid @cardBorder@; border-radius: 14px; }
QFrame[hairline="true"] { background: @sep@; border: none; max-height: 1px; min-height: 1px; }

QPushButton { background: @fill@; border: none; border-radius: 8px; padding: 7px 14px; font-weight: 500; }
QPushButton:hover { background: @fillHover@; }
QPushButton:pressed { background: @sep@; }
QPushButton:disabled { color: @text3@; }
QPushButton:focus { outline: none; }
QPushButton[primary="true"] { background: @accent@; color: white; font-weight: 600; font-size: 14px; padding: 10px 16px; border-radius: 10px; }
QPushButton[primary="true"]:hover { background: @accentHover@; }
QPushButton[primary="true"]:disabled { background: @accentOff@; color: rgba(255,255,255,0.75); }
QPushButton[link="true"] { background: transparent; color: @accentText@; padding: 5px 8px; }
QPushButton[link="true"]:hover { background: @accentSoft@; }
QPushButton[link="true"]:disabled { color: @text3@; }
QPushButton[link="true"][quiet="true"] { color: @text2@; }

QComboBox { background: @fill@; border: none; border-radius: 8px; padding: 6px 30px 6px 12px; min-height: 20px; font-weight: 500; }
QComboBox { combobox-popup: 0; }  /* список раскрывается вниз под полем, а не «наезжает» на выбранный пункт */
QComboBox:hover { background: @fillHover@; }
QComboBox:disabled { color: @text3@; }
QComboBox::drop-down { border: none; width: 26px; subcontrol-origin: padding; subcontrol-position: center right; }
QComboBox::down-arrow { image: url(@chevron@); width: 10px; height: 10px; }
QComboBoxPrivateContainer { background: @popup@; border: 1px solid @cardBorder@; border-radius: 10px; padding: 0; }
QComboBox QAbstractItemView { background: @popup@; color: @text@; border: none; border-radius: 8px; padding: 4px; outline: 0;
                              selection-background-color: @accent@; selection-color: white; }
QComboBox QAbstractItemView::item { min-height: 28px; padding: 2px 8px; border-radius: 6px; color: @text@; }
QComboBox QAbstractItemView::item:hover { background: @accentSoft@; }
QComboBox QAbstractItemView::item:selected { background: @accent@; color: white; }

QSlider { min-height: 22px; }
QSlider::groove:horizontal { height: 4px; background: @track@; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 2px; }
QSlider::sub-page:horizontal:disabled { background: @text3@; }
QSlider::handle:horizontal { background: white; border: 1px solid rgba(0,0,0,0.20); width: 18px; height: 18px; margin: -8px 0; border-radius: 10px; }
QSlider::handle:horizontal:disabled { background: #F2F2F7; border-color: rgba(0,0,0,0.10); }

QLineEdit { background: @fill@; border: 1px solid transparent; border-radius: 8px; padding: 6px 10px; selection-background-color: @accent@; }
QLineEdit:focus { border: 1px solid @accent@; }

QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar::handle:vertical { background: @scroll@; border-radius: 4px; min-height: 36px; margin: 0 2px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }
QScrollBar::handle:horizontal { background: @scroll@; border-radius: 4px; min-width: 36px; margin: 2px 0; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

QProgressBar { background: @track@; border: none; border-radius: 3px; min-height: 6px; max-height: 6px; text-align: center; color: transparent; }
QProgressBar::chunk { background: @accent@; border-radius: 3px; }

QListView { background: transparent; border: none; outline: 0; }
QTextBrowser, QTextEdit { background: transparent; border: none; }
QStatusBar { background: transparent; color: @text2@; }
QStatusBar::item { border: none; }
QToolTip { background: @popup@; color: @text@; border: 1px solid @cardBorder@; padding: 6px 9px; border-radius: 6px; }

QMenu { background: @popup@; border: 1px solid @cardBorder@; border-radius: 8px; padding: 5px; }
QMenu::item { padding: 6px 22px 6px 14px; border-radius: 5px; }
QMenu::item:selected { background: @accent@; color: white; }
QMenu::item:disabled { color: @text3@; }
QMenu::separator { height: 1px; background: @sep@; margin: 5px 8px; }

QTableWidget { background: @card@; border: 1px solid @cardBorder@; border-radius: 10px; gridline-color: @sep@; selection-background-color: @accentSoft@; selection-color: @text@; }
QHeaderView::section { background: transparent; border: none; border-bottom: 1px solid @sep@; padding: 7px 8px; color: @text2@; font-weight: 600; }
QTableWidget::item { padding: 4px 6px; }
QDialogButtonBox QPushButton { min-width: 76px; }
)";
    if (c.glass) s += "\nQWidget#glass { background: transparent; }\n";

    const QList<QPair<QString, QString>> tokens = {
        {"@cardBg@", cardBg},            {"@cardBorder@", css(c.cardBorder)}, {"@popup@", css(c.popup)},
        {"@text@", css(c.text)},         {"@text2@", css(c.text2)},       {"@text3@", css(c.text3)},
        {"@bg@", css(c.bg)},             {"@card@", css(c.card)},         {"@sep@", css(c.separator)},
        {"@fillHover@", css(c.fillHover)}, {"@fill@", css(c.fill)},
        {"@accentText@", css(c.accentText)}, {"@accentHover@", css(c.accentHover)}, {"@accentSoft@", css(c.accentSoft)},
        {"@accentOff@", css(withAlpha(c.accent, 0.45))}, {"@accent@", css(c.accent)},
        {"@track@", css(c.track)},       {"@scroll@", css(c.scroll)},     {"@chevron@", Icons::chevronFile(c.text2)}};
    for (const auto& t : tokens) s.replace(t.first, t.second);
    return s;
}

void Theme::refresh() {
    if (!app_) return;
    // UIC_THEME=light|dark принудительно задаёт схему (для проверки внешнего вида).
    const QString forced = qEnvironmentVariable("UIC_THEME").toLower();
    bool dark;
    if (forced == "dark") dark = true;
    else if (forced == "light") dark = false;
    else if (appearance_ == Appearance::Dark) dark = true;
    else if (appearance_ == Appearance::Light) dark = false;
    else dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    rebuild(dark);
    app_->setFont(font(13));
    app_->setStyleSheet(styleSheet());
    emit changed();
}

void Theme::apply(QApplication& app, Appearance appearance, bool glass) {
    app_ = &app;
    appearance_ = appearance;
    glass_ = glass;
#ifdef Q_OS_WIN
    realBlur_ = windowsBuild() >= 17134; // акрил появился в Windows 10 1803
#endif
    refresh();
    if (!connected_) {
        connected_ = true;
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
            if (appearance_ == Appearance::System) refresh();
        });
    }
}

void Theme::setAppearance(Appearance a) {
    if (a == appearance_) return;
    appearance_ = a;
    refresh();
}

void Theme::setGlass(bool on) {
    if (on == glass_) return;
    glass_ = on;
    refresh();
}

// ---------------- стеклянный фон ----------------

void Theme::paintBackdrop(QWidget* w) const {
    if (!colors_.glass) {
        // Окна создаются с прозрачным фоном (чтобы стекло включалось без перезапуска), а стили не рисуют
        // фон прозрачного окна — поэтому в обычном режиме закрашиваем его явно, иначе при смене темы
        // остаётся «чёрный» фон.
        QPainter p(w);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(w->rect(), colors_.bg);
        return;
    }
    const bool dark = colors_.dark;
    QPainter p(w);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    if (realBlur_) {
        // Окно прозрачное, размытие даёт система. Здесь — только однотонная вуаль для читаемости текста:
        // без цветных пятен и без градиентов.
        p.fillRect(w->rect(), dark ? QColor(30, 34, 70, 60) : QColor(255, 255, 255, 80));
    } else {
        // Запасной вариант без размытия системы: однотонный непрозрачный фон.
        p.fillRect(w->rect(), colors_.bg);
    }
}

// ---------------- оформление заголовка окна ----------------

void Theme::applyWindowChrome(QWidget* w) const {
#ifdef Q_OS_WIN
    if (!w || !w->isWindow()) return;
    const HWND hwnd = reinterpret_cast<HWND>(w->winId());
    const BOOL dark = colors_.dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof dark);

    const bool blur = colors_.glass && realBlur_;
    if (windowsBuild() >= 22621) {
        // Windows 11 22H2+: системный фон окна «акрил» (DWMSBT_TRANSIENTWINDOW) или без фона
        const MARGINS margins = blur ? MARGINS{-1, -1, -1, -1} : MARGINS{0, 0, 0, 0};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        const int backdrop = blur ? 3 /*DWMSBT_TRANSIENTWINDOW*/ : 1 /*DWMSBT_NONE*/;
        DwmSetWindowAttribute(hwnd, 38 /*DWMWA_SYSTEMBACKDROP_TYPE*/, &backdrop, sizeof backdrop);
    } else if (blur) {
        setAccentAcrylic(hwnd, true, colors_.dark ? QColor(12, 14, 32, 140) : QColor(255, 255, 255, 120));
    } else {
        setAccentAcrylic(hwnd, false, QColor());
    }

    if (windowsBuild() >= 22000) {
        // Заголовок: при размытии — прозрачный (виден акрил), иначе цвет темы
        const COLORREF none = 0xFFFFFFFF; // DWMWA_COLOR_DEFAULT
        const COLORREF capRef = blur ? none : RGB(colors_.bg.red(), colors_.bg.green(), colors_.bg.blue());
        const COLORREF txRef = RGB(colors_.text.red(), colors_.text.green(), colors_.text.blue());
        DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &capRef, sizeof capRef);
        DwmSetWindowAttribute(hwnd, 36 /*DWMWA_TEXT_COLOR*/, &txRef, sizeof txRef);
    }
#else
    Q_UNUSED(w)
#endif
}

} // namespace uic
