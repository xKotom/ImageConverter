#pragma once
#include <QLabel>
#include <QStringList>

class QMimeData;

namespace uic {

// Область перетаскивания файлов/папок. Сама ничего не импортирует — только отдаёт пути.
class DropArea : public QLabel {
    Q_OBJECT
public:
    explicit DropArea(QWidget* parent = nullptr);
    static QStringList pathsFromMime(const QMimeData* mime);

signals:
    void pathsDropped(const QStringList& paths);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void setHighlight(bool on);
};

} // namespace uic
