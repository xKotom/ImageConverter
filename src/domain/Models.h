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

// Результат быстрого зондирования декодером (без полного декодирования).
struct ImageProbe {
    int width = 0;
    int height = 0;
    int frameCount = 1;
    bool hasAlpha = false;
    bool hasIccProfile = false;
    int exifOrientation = 1; // 1..8
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

    bool extensionMismatch() const {
        ImageFormat hinted = formatFromExtension(extensionHint);
        return hinted != ImageFormat::Unknown && hinted != actualFormat;
    }
    bool animated() const { return frameCount > 1; }
};

enum class ConflictPolicy { Suffix, Replace, Skip };
enum class AnimationPolicy { FirstFrameWithWarning, Reject };

struct ConversionSettings {
    ImageFormat targetFormat = ImageFormat::Png;
    int quality = 90;                      // 1..100, для форматов с потерями
    fs::path outputDir;                    // пусто = рядом с исходником
    ConflictPolicy conflict = ConflictPolicy::Suffix;
    AnimationPolicy animation = AnimationPolicy::FirstFrameWithWarning;
    std::uint32_t backgroundRgb = 0xFFFFFF; // фон при потере прозрачности
};

struct ConversionPlan {
    bool allowed = false;
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
