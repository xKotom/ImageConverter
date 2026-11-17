#include "PreviewController.h"

#include <QMetaObject>
#include <QRunnable>

namespace uic {

PreviewController::PreviewController(const CodecRegistry& registry, PreviewCache& cache, QObject* parent)
    : QObject(parent), generator_(registry), cache_(cache) {
    qRegisterMetaType<std::shared_ptr<const ImageBuffer>>();
    pool_.setMaxThreadCount(1);
    timer_.setSingleShot(true);
    timer_.setInterval(150);
    connect(&timer_, &QTimer::timeout, this, &PreviewController::fire);
}

PreviewController::~PreviewController() {
    token_.cancel();
    pool_.clear();
    pool_.waitForDone();
}

void PreviewController::request(TaskId id, const fs::path& path, int maxDimension) {
    token_.cancel();             // старый запрос больше не нужен
    token_ = CancellationToken();
    ++serial_;
    pendingId_ = id;
    pendingPath_ = path;
    pendingDim_ = maxDimension;
    timer_.start();              // debounce: декодируем только когда выбор «устоялся»
}

void PreviewController::cancel() {
    timer_.stop();
    token_.cancel();
    ++serial_;
}

void PreviewController::fire() {
    const TaskId id = pendingId_;
    const fs::path path = pendingPath_;
    const int dim = pendingDim_;
    const std::uint64_t serial = serial_;
    const CancellationToken token = token_;

    const std::string key = PreviewCache::makeKey(path, dim, generator_.decoderVersionFor(path));
    if (auto hit = cache_.get(key)) {
        emit previewReady(id, hit);
        return;
    }
    pool_.start(QRunnable::create([this, id, path, dim, serial, token, key] {
        Result<ImageBuffer> r = makeError(ErrorCode::Internal, "Не выполнено");
        try { r = generator_.generate(path, dim, token); }
        catch (const std::exception& e) { r = makeError(ErrorCode::Internal, e.what()); }
        catch (...) { r = makeError(ErrorCode::Internal, "Неизвестная ошибка"); }

        std::shared_ptr<const ImageBuffer> img;
        if (r.ok()) img = std::make_shared<const ImageBuffer>(std::move(r.value()));
        const QString err = r.ok() ? QString() : QString::fromUtf8(r.error().message.c_str());
        const bool cancelled = !r.ok() && r.error().code == ErrorCode::Cancelled;

        QMetaObject::invokeMethod(this, [this, id, serial, key, img, err, cancelled] {
            if (serial != serial_ || cancelled) return; // устаревший результат — не показываем
            if (img) { cache_.put(key, img); emit previewReady(id, img); }
            else emit previewFailed(id, err);
        }, Qt::QueuedConnection);
    }));
}

} // namespace uic
