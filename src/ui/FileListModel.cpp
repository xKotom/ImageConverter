#include "FileListModel.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QApplication>
#include <QColor>
#include <QFileInfo>
#include <QLocale>

namespace uic {

namespace {
QString humanSize(std::uintmax_t b) {
    return QLocale().formattedDataSize(static_cast<qint64>(b), 1);
}
} // namespace

FileListModel::FileListModel(QueueManager& queue, QObject* parent) : QAbstractTableModel(parent), queue_(queue) {
    connect(&queue_, &QueueManager::taskAdded, this, &FileListModel::onAdded);
    connect(&queue_, &QueueManager::taskChanged, this, &FileListModel::onChanged);
    connect(&queue_, &QueueManager::taskRemoved, this, &FileListModel::onRemoved);
}

int FileListModel::rowCount(const QModelIndex& p) const { return p.isValid() ? 0 : static_cast<int>(ids_.size()); }
int FileListModel::columnCount(const QModelIndex& p) const { return p.isValid() ? 0 : ColCount; }

int FileListModel::rowOf(TaskId id) const {
    for (size_t i = 0; i < ids_.size(); ++i) if (ids_[i] == id) return static_cast<int>(i);
    return -1;
}

QString FileListModel::stateText(TaskState s) {
    switch (s) {
    case TaskState::Analyzing: return "Анализ…";
    case TaskState::Ready: return "Готов";
    case TaskState::Rejected: return "Отклонён";
    case TaskState::Queued: return "В очереди";
    case TaskState::Running: return "Выполняется";
    case TaskState::Cancelling: return "Отмена…";
    case TaskState::Succeeded: return "Успешно";
    case TaskState::Failed: return "Ошибка";
    case TaskState::Cancelled: return "Отменён";
    case TaskState::Skipped: return "Пропущен";
    }
    return {};
}

QVariant FileListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(ids_.size())) return {};
    const ConversionTask* t = queue_.task(ids_[static_cast<size_t>(index.row())]);
    if (!t) return {};

    if (role == TaskIdRole) return QVariant::fromValue<qulonglong>(t->id);
    if (role == StateRole) return static_cast<int>(t->state);
    if (role == ProgressRole) {
        if (t->state == TaskState::Queued || t->state == TaskState::Running || t->state == TaskState::Cancelling)
            return t->progress;
        return -1;
    }

    const FileInfo* info = t->info ? &*t->info : nullptr;
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColFile: return toQString(t->source.filename());
        case ColFormat:
            if (!info) return "—";
            return QString::fromUtf8(traits(info->actualFormat).label) + (info->animated() ? " (анимация)" : "");
        case ColSize:
            if (!info) return "—";
            return QString("%1×%2, %3").arg(info->width).arg(info->height).arg(humanSize(info->fileSize));
        case ColStatus: return stateText(t->state);
        case ColResult:
            if (t->state == TaskState::Succeeded)
                return toQString(t->result.outputPath.filename()) + " (" + humanSize(t->result.outputSize) + ")";
            return fromUtf8(t->message);
        }
    } else if (role == Qt::ToolTipRole) {
        QStringList lines;
        lines << toQString(t->source);
        if (info && info->extensionMismatch())
            lines << QString("⚠ Расширение «.%1» не соответствует содержимому").arg(QString::fromStdString(info->extensionHint));
        if (!t->message.empty()) lines << fromUtf8(t->message);
        for (const auto& w : t->warnings) lines << "⚠ " + fromUtf8(w);
        if (t->state == TaskState::Succeeded) lines << "Результат: " + toQString(t->result.outputPath);
        return lines.join('\n');
    } else if (role == Qt::ForegroundRole && index.column() == ColStatus) {
        switch (t->state) {
        case TaskState::Succeeded: return QColor(0x1b, 0x7f, 0x3b);
        case TaskState::Failed: case TaskState::Rejected: return QColor(0xc0, 0x2b, 0x2b);
        case TaskState::Cancelled: case TaskState::Skipped: return QColor(0x80, 0x80, 0x80);
        default: break;
        }
    } else if (role == Qt::DecorationRole && index.column() == ColFormat && info && info->extensionMismatch()) {
        return QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    }
    return {};
}

QVariant FileListModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    static const char* names[] = {"Файл", "Формат", "Размер", "Статус", "Результат / сообщение"};
    return section >= 0 && section < ColCount ? QVariant(QString::fromUtf8(names[section])) : QVariant();
}

void FileListModel::onAdded(TaskId id) {
    beginInsertRows({}, static_cast<int>(ids_.size()), static_cast<int>(ids_.size()));
    ids_.push_back(id);
    endInsertRows();
}

void FileListModel::onChanged(TaskId id) {
    const int r = rowOf(id);
    if (r >= 0) emit dataChanged(index(r, 0), index(r, ColCount - 1));
}

void FileListModel::onRemoved(TaskId id) {
    const int r = rowOf(id);
    if (r < 0) return;
    beginRemoveRows({}, r, r);
    ids_.erase(ids_.begin() + r);
    endRemoveRows();
}

void ProgressDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    const int progress = idx.data(FileListModel::ProgressRole).toInt();
    if (idx.column() != FileListModel::ColStatus || progress < 0) {
        QStyledItemDelegate::paint(p, opt, idx);
        return;
    }
    QStyleOptionProgressBar bar;
    bar.rect = opt.rect.adjusted(2, 3, -2, -3);
    bar.minimum = 0;
    bar.maximum = 100;
    bar.progress = progress;
    bar.text = idx.data().toString() + QString(" %1%").arg(progress);
    bar.textVisible = true;
    bar.state = QStyle::State_Enabled;
    QApplication::style()->drawControl(QStyle::CE_ProgressBar, &bar, p);
}

} // namespace uic
