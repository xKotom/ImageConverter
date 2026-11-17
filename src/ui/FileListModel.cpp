#include "FileListModel.h"
#include "Theme.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QFontMetrics>
#include <QLocale>
#include <QPainter>

namespace uic {

namespace {

QString humanSize(std::uintmax_t b) { return QLocale().formattedDataSize(static_cast<qint64>(b), 1); }

QString badgeFor(const ConversionTask& t) {
    if (t.info) {
        switch (t.info->actualFormat) {
        case ImageFormat::Jpeg: return "JPG";
        case ImageFormat::Tiff: return "TIF";
        case ImageFormat::Heif: return "HEIC";
        default: return QString::fromUtf8(traits(t.info->actualFormat).label);
        }
    }
    QString ext = toQString(t.source.extension()).mid(1).toUpper();
    return ext.isEmpty() ? "?" : ext.left(4);
}

} // namespace

FileListModel::FileListModel(QueueManager& queue, QObject* parent) : QAbstractListModel(parent), queue_(queue) {
    connect(&queue_, &QueueManager::taskAdded, this, &FileListModel::onAdded);
    connect(&queue_, &QueueManager::taskChanged, this, &FileListModel::onChanged);
    connect(&queue_, &QueueManager::taskRemoved, this, &FileListModel::onRemoved);
}

int FileListModel::rowCount(const QModelIndex& p) const { return p.isValid() ? 0 : static_cast<int>(ids_.size()); }

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
    case TaskState::Succeeded: return "Готово";
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
    const FileInfo* info = t->info ? &*t->info : nullptr;

    switch (role) {
    case Qt::DisplayRole: return toQString(t->source.filename());
    case TaskIdRole: return QVariant::fromValue<qulonglong>(t->id);
    case StateRole: return static_cast<int>(t->state);
    case StatusTextRole: return stateText(t->state);
    case BadgeRole: return badgeFor(*t);
    case ProgressRole:
        if (t->state == TaskState::Queued || t->state == TaskState::Running || t->state == TaskState::Cancelling)
            return t->progress;
        return -1;
    case WarnRole:
        return !t->warnings.empty() || (info && info->extensionMismatch());
    case SubtitleRole: {
        if (t->state == TaskState::Succeeded) {
            QString s = "→ " + toQString(t->result.outputPath.filename()) + " · " + humanSize(t->result.outputSize);
            return s;
        }
        if (!t->message.empty() && t->state != TaskState::Skipped) return fromUtf8(t->message);
        if (t->state == TaskState::Skipped) return fromUtf8(t->message);
        if (!info) return t->state == TaskState::Analyzing ? QString("Анализ…") : QString();
        QString s = QString::fromUtf8(traits(info->actualFormat).label);
        s += QString(" · %1×%2 · %3").arg(info->width).arg(info->height).arg(humanSize(info->fileSize));
        if (info->animated()) s += QString(" · %1 кадр.").arg(info->frameCount);
        return s;
    }
    case Qt::ToolTipRole: {
        QStringList lines;
        lines << toQString(t->source);
        if (info && info->extensionMismatch())
            lines << QString("⚠ Расширение «.%1» не соответствует содержимому").arg(QString::fromStdString(info->extensionHint));
        if (!t->message.empty()) lines << fromUtf8(t->message);
        for (const auto& w : t->warnings) lines << "⚠ " + fromUtf8(w);
        if (t->state == TaskState::Succeeded) lines << "Результат: " + toQString(t->result.outputPath);
        return lines.join('\n');
    }
    default: return {};
    }
}

void FileListModel::onAdded(TaskId id) {
    beginInsertRows({}, static_cast<int>(ids_.size()), static_cast<int>(ids_.size()));
    ids_.push_back(id);
    endInsertRows();
}

void FileListModel::onChanged(TaskId id) {
    const int r = rowOf(id);
    if (r >= 0) emit dataChanged(index(r), index(r));
}

void FileListModel::onRemoved(TaskId id) {
    const int r = rowOf(id);
    if (r < 0) return;
    beginRemoveRows({}, r, r);
    ids_.erase(ids_.begin() + r);
    endRemoveRows();
}

// ---------------- отрисовка строки ----------------

void RowDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    const Theme& theme = Theme::instance();
    const ThemeColors& c = theme.colors();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const QRectF row = QRectF(opt.rect).adjusted(4, 2, -4, -2);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = opt.state & QStyle::State_MouseOver;
    if (selected) {
        p->setPen(Qt::NoPen);
        p->setBrush(c.dark ? withAlpha(c.accent, 0.35) : withAlpha(c.accent, 0.14));
        p->drawRoundedRect(row, 10, 10);
    } else if (hovered) {
        p->setPen(Qt::NoPen);
        p->setBrush(withAlpha(c.text, 0.05));
        p->drawRoundedRect(row, 10, 10);
    }

    const auto state = static_cast<TaskState>(idx.data(FileListModel::StateRole).toInt());
    const int progress = idx.data(FileListModel::ProgressRole).toInt();
    const bool warn = idx.data(FileListModel::WarnRole).toBool();

    // --- значок формата ---
    const QRectF badge(row.left() + 8, row.center().y() - 20, 40, 40);
    p->setPen(Qt::NoPen);
    p->setBrush(withAlpha(c.accent, c.dark ? 0.25 : 0.12));
    p->drawRoundedRect(badge, 10, 10);
    p->setPen(c.accentText);
    QFont bf = theme.font(10, QFont::Bold);
    p->setFont(bf);
    p->drawText(badge, Qt::AlignCenter, idx.data(FileListModel::BadgeRole).toString());

    // --- статус справа ---
    QColor statusColor = c.text2;
    switch (state) {
    case TaskState::Succeeded: statusColor = c.success; break;
    case TaskState::Failed: statusColor = c.danger; break;
    case TaskState::Rejected: statusColor = c.warning; break;
    case TaskState::Ready: statusColor = c.accentText; break;
    default: break;
    }
    const QString statusText = idx.data(FileListModel::StatusTextRole).toString();
    qreal rightEdge = row.right() - 12;
    if (progress >= 0) {
        const QRectF bar(rightEdge - 110, row.center().y() - 3, 80, 6);
        p->setPen(Qt::NoPen);
        p->setBrush(c.track);
        p->drawRoundedRect(bar, 3, 3);
        p->setBrush(c.accent);
        p->drawRoundedRect(QRectF(bar.left(), bar.top(), std::max(6.0, bar.width() * progress / 100.0), bar.height()), 3, 3);
        p->setPen(c.text2);
        p->setFont(theme.font(12, QFont::Medium));
        p->drawText(QRectF(bar.right() + 6, row.top(), 34, row.height()), Qt::AlignVCenter | Qt::AlignLeft,
                    QString("%1%").arg(progress));
        rightEdge = bar.left() - 12;
    } else {
        const QFont pf = theme.font(12, QFont::DemiBold);
        const QFontMetrics fm(pf);
        const qreal w = fm.horizontalAdvance(statusText) + 22;
        const QRectF pill(rightEdge - w, row.center().y() - 11, w, 22);
        p->setPen(Qt::NoPen);
        p->setBrush(withAlpha(statusColor, c.dark ? 0.22 : 0.13));
        p->drawRoundedRect(pill, 11, 11);
        p->setPen(statusColor);
        p->setFont(pf);
        p->drawText(pill, Qt::AlignCenter, statusText);
        rightEdge = pill.left() - 10;
    }
    if (warn) {
        const QRectF w(rightEdge - 18, row.center().y() - 9, 18, 18);
        p->setPen(Qt::NoPen);
        p->setBrush(c.warning);
        p->drawEllipse(w);
        p->setPen(Qt::white);
        p->setFont(theme.font(12, QFont::Bold));
        p->drawText(w, Qt::AlignCenter, "!");
        rightEdge = w.left() - 10;
    }

    // --- название и подпись ---
    const qreal textLeft = badge.right() + 12;
    const QRectF textRect(textLeft, row.top(), std::max(40.0, rightEdge - textLeft), row.height());
    const QFont tf = theme.font(14, QFont::DemiBold);
    const QFont sf = theme.font(12, QFont::Normal);
    const QFontMetrics tfm(tf), sfm(sf);
    p->setPen(c.text);
    p->setFont(tf);
    const qreal blockH = tfm.height() + sfm.height() + 2;
    const qreal top = textRect.center().y() - blockH / 2;
    p->drawText(QPointF(textRect.left(), top + tfm.ascent()),
                tfm.elidedText(idx.data(Qt::DisplayRole).toString(), Qt::ElideMiddle, static_cast<int>(textRect.width())));
    p->setFont(sf);
    p->setPen(state == TaskState::Failed || state == TaskState::Rejected ? statusColor : c.text2);
    p->drawText(QPointF(textRect.left(), top + tfm.height() + 2 + sfm.ascent()),
                sfm.elidedText(idx.data(FileListModel::SubtitleRole).toString(), Qt::ElideRight,
                               static_cast<int>(textRect.width())));
    p->restore();
}

} // namespace uic
