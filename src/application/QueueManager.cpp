#include "QueueManager.h"
#include "FileImporter.h"

#include <QMetaObject>
#include <QRunnable>

namespace uic {

namespace {
bool isActive(TaskState s) {
    return s == TaskState::Analyzing || s == TaskState::Queued || s == TaskState::Running ||
           s == TaskState::Cancelling;
}
bool isFinished(TaskState s) {
    return s == TaskState::Succeeded || s == TaskState::Failed || s == TaskState::Cancelled ||
           s == TaskState::Skipped || s == TaskState::Rejected;
}
} // namespace

QueueManager::QueueManager(const CodecRegistry& registry, QObject* parent)
    : QObject(parent), registry_(registry), analyzer_(registry), planner_(registry), engine_(registry) {
    pool_.setMaxThreadCount(2); // ограниченная параллельность: большие снимки занимают много памяти
    analysisPool_.setMaxThreadCount(2);
}

QueueManager::~QueueManager() {
    cancelAll();
    pool_.clear();
    analysisPool_.clear();
    pool_.waitForDone();
    analysisPool_.waitForDone();
}

void QueueManager::setMaxParallel(int n) { pool_.setMaxThreadCount(std::max(1, n)); }

ConversionTask* QueueManager::find(TaskId id) {
    for (auto& t : tasks_) if (t.id == id) return &t;
    return nullptr;
}

const ConversionTask* QueueManager::task(TaskId id) const {
    for (const auto& t : tasks_) if (t.id == id) return &t;
    return nullptr;
}

bool QueueManager::isBusy() const {
    if (pendingAnalysis_ > 0) return true;
    for (const auto& t : tasks_) if (isActive(t.state)) return true;
    return false;
}

int QueueManager::activeCount() const {
    int n = 0;
    for (const auto& t : tasks_) if (isActive(t.state)) ++n;
    return n;
}

void QueueManager::checkIdle() {
    if (isBusy()) { wasBusy_ = true; return; }
    if (wasBusy_) { wasBusy_ = false; emit queueIdle(); }
}

// ---------- импорт и анализ ----------

void QueueManager::importPaths(const std::vector<fs::path>& paths) {
    ++pendingAnalysis_;
    wasBusy_ = true;
    analysisPool_.start(QRunnable::create([this, paths] {
        std::vector<fs::path> list;
        try { list = FileImporter::collect(paths); } catch (...) {}
        QMetaObject::invokeMethod(this, [this, list = std::move(list)] {
            --pendingAnalysis_;
            int added = 0, dups = 0;
            for (const auto& p : list) {
                const std::string key = FileImporter::pathKey(p);
                if (keys_.count(key)) { ++dups; continue; }
                addTask(p);
                ++added;
            }
            emit importFinished(added, dups);
            checkIdle();
        }, Qt::QueuedConnection);
    }));
}

void QueueManager::addTask(const fs::path& path) {
    ConversionTask t;
    t.id = nextId_++;
    t.source = path;
    t.state = TaskState::Analyzing;
    keys_.insert(FileImporter::pathKey(path));
    tasks_.push_back(std::move(t));
    const TaskId id = tasks_.back().id;
    emit taskAdded(id);
    analyzeAsync(id);
}

void QueueManager::analyzeAsync(TaskId id) {
    const ConversionTask* t = task(id);
    if (!t) return;
    const fs::path path = t->source;
    analysisPool_.start(QRunnable::create([this, id, path] {
        Result<FileInfo> r = makeError(ErrorCode::Internal, "Анализ не выполнен");
        try {
            r = analyzer_.analyze(path);
        } catch (const std::exception& e) {
            r = makeError(ErrorCode::Internal, std::string("Внутренняя ошибка анализа: ") + e.what());
        } catch (...) {
            r = makeError(ErrorCode::Internal, "Неизвестная ошибка анализа");
        }
        QMetaObject::invokeMethod(this, [this, id, r = std::move(r)] { onAnalyzed(id, r); },
                                  Qt::QueuedConnection);
    }));
}

void QueueManager::onAnalyzed(TaskId id, const Result<FileInfo>& r) {
    ConversionTask* t = find(id);
    if (!t) { checkIdle(); return; } // удалена, пока анализировалась
    if (r.ok()) {
        t->info = r.value();
        t->state = TaskState::Ready;
        t->message.clear();
    } else {
        switch (r.error().code) {
        case ErrorCode::UnsupportedFormat: case ErrorCode::NoDecoder: case ErrorCode::NotFound:
        case ErrorCode::InvalidInput: case ErrorCode::TooLarge:
            t->state = TaskState::Rejected; break;
        default: t->state = TaskState::Failed; break;
        }
        t->info.reset();
        t->message = r.error().message;
    }
    emit taskChanged(id);
    checkIdle();
}

// ---------- запуск ----------

void QueueManager::dispatch(ConversionTask& t, const ConversionSettings& s) {
    t.settings = s; // снимок: дальнейшие изменения UI на задачу не влияют
    t.token = CancellationToken();
    t.progress = 0;
    t.message.clear();
    t.result = ConversionResult{};

    ConversionPlan plan = planner_.plan(*t.info, s);
    t.warnings = plan.warnings;
    if (!plan.allowed) {
        t.state = TaskState::Rejected;
        t.message = plan.rejectReason;
        emit taskChanged(t.id);
        return;
    }
    t.state = TaskState::Queued;
    emit taskChanged(t.id);

    const TaskId id = t.id;
    const FileInfo info = *t.info;
    const CancellationToken token = t.token;
    wasBusy_ = true;
    pool_.start(QRunnable::create([this, id, info, s, token] {
        ConversionResult res;
        if (token.cancelled()) {
            res.error = makeError(ErrorCode::Cancelled, "Операция отменена");
        } else {
            QMetaObject::invokeMethod(this, [this, id] { onProgress(id, 1); }, Qt::QueuedConnection);
            int last = -1;
            res = engine_.convert(info, s, token, [this, id, &last](int p) {
                if (p == last) return;
                last = p;
                QMetaObject::invokeMethod(this, [this, id, p] { onProgress(id, p); }, Qt::QueuedConnection);
            });
        }
        QMetaObject::invokeMethod(this, [this, id, res = std::move(res)] { onFinished(id, res); },
                                  Qt::QueuedConnection);
    }));
}

void QueueManager::onProgress(TaskId id, int percent) {
    ConversionTask* t = find(id);
    if (!t) return;
    if (t->state == TaskState::Queued) t->state = TaskState::Running;
    if (t->state == TaskState::Running) t->progress = percent;
    emit taskChanged(id);
}

void QueueManager::onFinished(TaskId id, const ConversionResult& r) {
    ConversionTask* t = find(id);
    if (!t) { checkIdle(); return; }
    t->result = r;
    if (r.success) {
        t->state = r.skipped ? TaskState::Skipped : TaskState::Succeeded;
        t->progress = 100;
        t->warnings = r.warnings;
        t->message = r.skipped ? "Пропущено: файл с таким именем уже существует" : std::string();
    } else if (r.error && r.error->code == ErrorCode::Cancelled) {
        t->state = TaskState::Cancelled;
        t->progress = 0;
        t->message = "Отменено";
    } else {
        t->state = TaskState::Failed;
        t->progress = 0;
        t->message = r.error ? r.error->message : "Неизвестная ошибка";
    }
    emit taskChanged(id);
    checkIdle();
}

void QueueManager::start(const std::vector<TaskId>& ids, const ConversionSettings& s) {
    for (TaskId id : ids) {
        ConversionTask* t = find(id);
        if (!t || !t->info) continue; // без успешного анализа запускать нечего
        switch (t->state) {
        case TaskState::Ready: case TaskState::Rejected: case TaskState::Failed:
        case TaskState::Cancelled: case TaskState::Skipped:
            dispatch(*t, s); break;
        default: break; // Succeeded — только явный retry
        }
    }
}

void QueueManager::startAllReady(const ConversionSettings& s) {
    std::vector<TaskId> ids;
    for (const auto& t : tasks_) if (t.state == TaskState::Ready) ids.push_back(t.id);
    start(ids, s);
}

void QueueManager::retry(const std::vector<TaskId>& ids, const ConversionSettings& fallback) {
    for (TaskId id : ids) {
        ConversionTask* t = find(id);
        if (!t || !isFinished(t->state)) continue;
        if (!t->info) { // анализ не удался — повторяем анализ
            t->state = TaskState::Analyzing;
            t->message.clear();
            emit taskChanged(id);
            wasBusy_ = true;
            analyzeAsync(id);
            continue;
        }
        const ConversionSettings s =
            (t->state == TaskState::Rejected || !t->settings) ? fallback : *t->settings;
        dispatch(*t, s);
    }
}

// ---------- отмена и удаление ----------

void QueueManager::cancel(TaskId id) {
    ConversionTask* t = find(id);
    if (!t) return;
    if (t->state == TaskState::Queued || t->state == TaskState::Running) {
        t->token.cancel();
        t->state = TaskState::Cancelling;
        emit taskChanged(id);
    }
}

void QueueManager::cancelAll() {
    for (auto& t : tasks_) {
        if (t.state == TaskState::Queued || t.state == TaskState::Running) {
            t.token.cancel();
            t.state = TaskState::Cancelling;
            emit taskChanged(t.id);
        }
    }
}

void QueueManager::remove(TaskId id) {
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        if (it->id != id) continue;
        it->token.cancel(); // если задача активна, результат будет проигнорирован
        keys_.erase(FileImporter::pathKey(it->source));
        tasks_.erase(it);
        emit taskRemoved(id);
        checkIdle();
        return;
    }
}

void QueueManager::clearFinished() {
    std::vector<TaskId> ids;
    for (const auto& t : tasks_) if (isFinished(t.state)) ids.push_back(t.id);
    for (TaskId id : ids) remove(id);
}

} // namespace uic
