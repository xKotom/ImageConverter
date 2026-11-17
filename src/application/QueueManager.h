#pragma once
#include "CodecRegistry.h"
#include "ConversionEngine.h"
#include "ConversionPlanner.h"
#include "FileAnalyzer.h"

#include <QObject>
#include <QThreadPool>

#include <set>
#include <unordered_map>

namespace uic {

// Владеет задачами и их состояниями; всю тяжёлую работу (анализ, конвертация) выполняет
// в пулах потоков. Все публичные методы и сигналы — только в GUI-потоке.
class QueueManager : public QObject {
    Q_OBJECT
public:
    explicit QueueManager(const CodecRegistry& registry, QObject* parent = nullptr);
    ~QueueManager() override;

    // Асинхронно разворачивает пути (в т.ч. папки), убирает повторы, создаёт задачи и анализирует их.
    void importPaths(const std::vector<fs::path>& paths);

    // Ставит в очередь задачи в состояниях Ready/Rejected(по плану)/Failed/Cancelled/Skipped.
    void start(const std::vector<TaskId>& ids, const ConversionSettings& settings);
    void startAllReady(const ConversionSettings& settings);
    // Явный повтор (в т.ч. успешных): со снимком прежних настроек или с переданными.
    void retry(const std::vector<TaskId>& ids, const ConversionSettings& fallback);

    void cancel(TaskId id);
    void cancelAll();
    void remove(TaskId id);     // активные сначала отменяются
    void clearFinished();

    const ConversionTask* task(TaskId id) const;
    const std::vector<ConversionTask>& tasks() const { return tasks_; }
    void setMaxParallel(int n);
    int maxParallel() const { return pool_.maxThreadCount(); }
    bool isBusy() const;
    int activeCount() const;

    const ConversionPlanner& planner() const { return planner_; }

signals:
    void taskAdded(uic::TaskId id);
    void taskChanged(uic::TaskId id);
    void taskRemoved(uic::TaskId id);
    void importFinished(int added, int duplicates);
    void queueIdle();

private:
    ConversionTask* find(TaskId id);
    void addTask(const fs::path& path);
    void analyzeAsync(TaskId id);
    void dispatch(ConversionTask& t, const ConversionSettings& s);
    void onAnalyzed(TaskId id, const Result<FileInfo>& r);
    void onProgress(TaskId id, int percent);
    void onFinished(TaskId id, const ConversionResult& r);
    void checkIdle();

    const CodecRegistry& registry_;
    FileAnalyzer analyzer_;
    ConversionPlanner planner_;
    ConversionEngine engine_;
    QThreadPool pool_;         // конвертация
    QThreadPool analysisPool_; // анализ/импорт
    std::vector<ConversionTask> tasks_;
    std::set<std::string> keys_;
    TaskId nextId_ = 1;
    int pendingAnalysis_ = 0;
    bool wasBusy_ = false;
};

} // namespace uic
