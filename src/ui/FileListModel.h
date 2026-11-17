#pragma once
#include "application/QueueManager.h"

#include <QAbstractListModel>
#include <QStyledItemDelegate>

namespace uic {

// Отображает очередь. Никакого декодирования в data(): только готовые данные задач.
class FileListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        TaskIdRole = Qt::UserRole + 1,
        StateRole,
        ProgressRole,   // 0..100 у активных задач, иначе -1
        SubtitleRole,   // «PNG · 1920×1080 · 2,4 МБ» / результат / причина ошибки
        BadgeRole,      // короткое имя формата для значка слева
        StatusTextRole,
        WarnRole        // есть предупреждения (в т.ч. несовпадение расширения)
    };

    explicit FileListModel(QueueManager& queue, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    TaskId idAt(int row) const { return ids_.at(static_cast<size_t>(row)); }
    int rowOf(TaskId id) const;

    static QString stateText(TaskState s);

private slots:
    void onAdded(uic::TaskId id);
    void onChanged(uic::TaskId id);
    void onRemoved(uic::TaskId id);

private:
    QueueManager& queue_;
    std::vector<TaskId> ids_;
};

// Строка ленты в стиле Finder/Фото: значок формата, название, подпись и статус справа.
class RowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {200, 60}; }
};

} // namespace uic
