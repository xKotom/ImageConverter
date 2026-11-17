#pragma once
#include <QStringList>
#include <QWidget>

class QLabel;
class QMimeData;

namespace uic {

// Пустое состояние окна и зона перетаскивания: значок, подсказка и кнопки добавления.
// Сама ничего не импортирует — только сообщает о пути/действии.
class DropArea : public QWidget {
    Q_OBJECT
public:
    explicit DropArea(QWidget* parent = nullptr);
    static QStringList pathsFromMime(const QMimeData* mime);

signals:
    void pathsDropped(const QStringList& paths);
    void addFilesRequested();
    void addFolderRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void refreshIcon();
    QLabel* icon_ = nullptr;
    bool highlight_ = false;
};

} // namespace uic
