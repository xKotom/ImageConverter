#pragma once
// Доменные модели: не зависят ни от Qt, ни от Windows API.
#include "ImageFormat.h"
#include "Result.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace uic {

namespace fs = std::filesystem;
using TaskId = std::uint64_t;
using ProgressSink = std::function<void(int /*0..100*/)>;

class CancellationToken {
public:
    CancellationToken() : flag_(std::make_shared<std::atomic<bool>>(false)) {}
    void cancel() const { flag_->store(true); }
    bool cancelled() const { return flag_->load(); }
private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

// Декодированные пиксели: RGBA8888, не premultiplied, ориентация уже применена.
struct ImageBuffer {
    int width = 0;
    int height = 0;
    bool hasAlpha = false;
    std::vector<std::uint8_t> rgba;       // width * height * 4
    std::vector<std::uint8_t> iccProfile; // пусто, если профиля нет
    int stride() const { return width * 4; }
    bool empty() const { return width <= 0 || height <= 0 || rgba.empty(); }
    std::size_t byteSize() const { return rgba.size() + iccProfile.size(); }
};

// Анимация / многостраничное изображение: полноразмерные кадры + задержки.
struct ImageSequence {
    std::vector<ImageBuffer> frames;
    std::vector<int> delaysMs; // по одному на кадр (0 для статичных страниц)
    int loopCount = 0;         // 0 = бесконечно
    std::size_t byteSize() const {
        std::size_t n = 0;
        for (const auto& f : frames) n += f.byteSize();
        return n;
    }
};

// Результат быстрого зондирования декодером (без полного декодирования).
struct ImageProbe {
    int width = 0;
    int height = 0;
    int frameCount = 1;
    bool hasAlpha = false;
    bool hasIccProfile = false;
    int exifOrientation = 1; // 1..8
};

// Сведения о метаданных исходного файла (EXIF/XMP/IPTC).
struct MetadataInfo {
    bool hasExif = false;
    bool hasXmp = false;
    bool hasIptc = false;
    bool hasGps = false;
    bool any() const { return hasExif || hasXmp || hasIptc; }
};

struct FileInfo {
    fs::path path;
    ImageFormat actualFormat = ImageFormat::Unknown; // по содержимому
    std::string extensionHint;                       // как в имени файла, без точки
    std::uintmax_t fileSize = 0;
    int width = 0, height = 0;
    int exifOrientation = 1;
    bool hasAlpha = false;
    int frameCount = 1;
    bool hasIccProfile = false;
    MetadataInfo metadata;

    bool extensionMismatch() const {
        ImageFormat hinted = formatFromExtension(extensionHint);
        return hinted != ImageFormat::Unknown && hinted != actualFormat;
    }
    bool animated() const { return frameCount > 1; }
};

enum class ConflictPolicy { Suffix, Replace, Skip };
// AllFrames: сохранить все кадры, если целевой формат поддерживает последовательности;
// иначе — по запасной политике (первый кадр с предупреждением) либо отказ.
enum class AnimationPolicy { AllFrames, FirstFrameWithWarning, Reject };
// Что делать с EXIF/XMP/IPTC. GPS переносится только по явному выбору.
enum class MetadataPolicy { Strip, KeepWithoutGps, KeepAll };

struct ConversionSettings {
    ImageFormat targetFormat = ImageFormat::Png;
    int quality = 90;                      // 1..100, для форматов с потерями
    fs::path outputDir;                    // пусто = рядом с исходником
    ConflictPolicy conflict = ConflictPolicy::Suffix;
    AnimationPolicy animation = AnimationPolicy::AllFrames;
    MetadataPolicy metadata = MetadataPolicy::KeepWithoutGps;
    std::uint32_t backgroundRgb = 0xFFFFFF; // фон при потере прозрачности
};

struct ConversionPlan {
    bool allowed = false;
    bool allFrames = false;         // будут сохранены все кадры
    bool transferMetadata = false;  // метаданные будут перенесены
    int framesToWrite = 1;
    std::string rejectReason;
    std::vector<std::string> warnings;
};

struct ConversionResult {
    bool success = false;
    bool skipped = false;
    fs::path outputPath;
    std::uintmax_t outputSize = 0;
    std::vector<std::string> warnings;
    std::optional<Error> error;
    long long durationMs = 0;
};

enum class TaskState {
    Analyzing, Ready, Rejected, Queued, Running, Cancelling,
    Succeeded, Failed, Cancelled, Skipped
};

struct ConversionTask {
    TaskId id = 0;
    fs::path source;
    std::optional<FileInfo> info;
    std::optional<ConversionSettings> settings; // снимок на момент запуска
    TaskState state = TaskState::Analyzing;
    int progress = 0;
    std::vector<std::string> warnings;
    std::string message;                        // причина отказа/ошибки
    ConversionResult result;
    CancellationToken token;
};

} // namespace uic
