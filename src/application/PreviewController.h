#pragma once
#include "PreviewCache.h"
#include "ThumbnailGenerator.h"

#include <QObject>
#include <QThreadPool>
#include <QTimer>

namespace uic {

// Debounce выбора, отмена устаревшего запроса, привязка результата к идентификатору строки.
class PreviewController : public QObject {
    Q_OBJECT
public:
    PreviewController(const CodecRegistry& registry, PreviewCache& cache, QObject* parent = nullptr);
    ~PreviewController() override;

    void setDebounceMs(int ms) { timer_.setInterval(ms); }
    void request(TaskId id, const fs::path& path, int maxDimension = 640);
    void cancel();

signals:
    void previewReady(uic::TaskId id, std::shared_ptr<const uic::ImageBuffer> image);
    void previewFailed(uic::TaskId id, const QString& message);

private:
    void fire();

    ThumbnailGenerator generator_;
    PreviewCache& cache_;
    QThreadPool pool_;
    QTimer timer_;
    TaskId pendingId_ = 0;
    fs::path pendingPath_;
    int pendingDim_ = 640;
    std::uint64_t serial_ = 0;
    CancellationToken token_;
};

} // namespace uic

Q_DECLARE_METATYPE(std::shared_ptr<const uic::ImageBuffer>)
