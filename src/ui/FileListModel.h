#pragma once
#include "application/QueueManager.h"

#include <QAbstractTableModel>
#include <QStyledItemDelegate>

namespace uic {

// Отображает очередь. Никакого декодирования в data(): только готовые данные задач.
class FileListModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColFile, ColFormat, ColSize, ColStatus, ColResult, ColCount };
    enum Role { TaskIdRole = Qt::UserRole + 1, ProgressRole, StateRole };

    explicit FileListModel(QueueManager& queue, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

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

// Рисует полосу прогресса в колонке статуса для выполняющихся задач.
class ProgressDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;
};

} // namespace uic
