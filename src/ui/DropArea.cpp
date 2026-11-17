#include "DropArea.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

namespace uic {

DropArea::DropArea(QWidget* parent) : QLabel(parent) {
    setAcceptDrops(true);
    setAlignment(Qt::AlignCenter);
    setText("Перетащите сюда изображения или папки\nлибо воспользуйтесь кнопкой «Добавить файлы…»");
    setMinimumHeight(64);
    setHighlight(false);
}

QStringList DropArea::pathsFromMime(const QMimeData* mime) {
    QStringList out;
    if (!mime || !mime->hasUrls()) return out;
    for (const QUrl& u : mime->urls())
        if (u.isLocalFile()) out << u.toLocalFile();
    return out;
}

void DropArea::setHighlight(bool on) {
    setStyleSheet(on ? "QLabel{border:2px dashed #2a7de1;border-radius:8px;background:rgba(42,125,225,0.10);}"
                     : "QLabel{border:2px dashed #888;border-radius:8px;color:#666;}");
}

void DropArea::dragEnterEvent(QDragEnterEvent* e) {
    if (!pathsFromMime(e->mimeData()).isEmpty()) { e->acceptProposedAction(); setHighlight(true); }
}

void DropArea::dragLeaveEvent(QDragLeaveEvent*) { setHighlight(false); }

void DropArea::dropEvent(QDropEvent* e) {
    setHighlight(false);
    const QStringList paths = pathsFromMime(e->mimeData());
    if (paths.isEmpty()) return;
    e->acceptProposedAction();
    emit pathsDropped(paths);
}

} // namespace uic
