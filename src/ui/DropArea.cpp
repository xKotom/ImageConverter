#include "DropArea.h"
#include "Icons.h"
#include "Theme.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace uic {

DropArea::DropArea(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setMinimumHeight(250);
    auto* v = new QVBoxLayout(this);
    v->setAlignment(Qt::AlignCenter);
    v->setSpacing(4);

    icon_ = new QLabel;
    icon_->setAlignment(Qt::AlignCenter);
    icon_->setFixedSize(60, 60);
    v->addWidget(icon_, 0, Qt::AlignHCenter);
    v->addSpacing(6);

    auto* title = new QLabel("Перетащите изображения сюда");
    title->setProperty("role", "heading");
    title->setAlignment(Qt::AlignCenter);
    v->addWidget(title);

    auto* hint = new QLabel("JPEG, PNG, WEBP, GIF, TIFF, BMP, HEIC, AVIF");
    hint->setProperty("role", "caption");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    v->addWidget(hint);
    v->addSpacing(8);

    auto* add = new QPushButton("Добавить файлы…");
    add->setProperty("primary", true);
    add->setMinimumWidth(190);
    add->setCursor(Qt::PointingHandCursor);
    v->addWidget(add, 0, Qt::AlignHCenter);
    auto* folder = new QPushButton("Добавить папку…");
    folder->setProperty("link", true);
    folder->setCursor(Qt::PointingHandCursor);
    v->addWidget(folder, 0, Qt::AlignHCenter);

    connect(add, &QPushButton::clicked, this, &DropArea::addFilesRequested);
    connect(folder, &QPushButton::clicked, this, &DropArea::addFolderRequested);
    connect(&Theme::instance(), &Theme::changed, this, [this] { refreshIcon(); update(); });
    refreshIcon();
}

void DropArea::refreshIcon() {
    const ThemeColors& c = Theme::instance().colors();
    QPixmap pm(QSize(60, 60) * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c.accentSoft);
    p.drawEllipse(QRectF(0, 0, 60, 60));
    p.drawPixmap(QRect(15, 15, 30, 30), Icons::pixmap(Icons::Kind::TrayDown, 30, c.accent, 2));
    p.end();
    icon_->setPixmap(pm);
}

QStringList DropArea::pathsFromMime(const QMimeData* mime) {
    QStringList out;
    if (!mime || !mime->hasUrls()) return out;
    for (const QUrl& u : mime->urls())
        if (u.isLocalFile()) out << u.toLocalFile();
    return out;
}

void DropArea::paintEvent(QPaintEvent*) {
    const ThemeColors& c = Theme::instance().colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    p.setBrush(highlight_ ? c.accentSoft : c.card);
    QPen pen(highlight_ ? c.accent : c.text3, 1.5, Qt::CustomDashLine);
    pen.setDashPattern({5, 5});
    p.setPen(pen);
    p.drawRoundedRect(r, 16, 16);
}

void DropArea::dragEnterEvent(QDragEnterEvent* e) {
    if (!pathsFromMime(e->mimeData()).isEmpty()) { e->acceptProposedAction(); highlight_ = true; update(); }
}

void DropArea::dragLeaveEvent(QDragLeaveEvent*) { highlight_ = false; update(); }

void DropArea::dropEvent(QDropEvent* e) {
    highlight_ = false;
    update();
    const QStringList paths = pathsFromMime(e->mimeData());
    if (paths.isEmpty()) return;
    e->acceptProposedAction();
    emit pathsDropped(paths);
}

} // namespace uic
