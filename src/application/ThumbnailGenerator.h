#pragma once
#include "CodecRegistry.h"

namespace uic {

// Создаёт уменьшенную копию через «scaled read» декодера — без полного декодирования в UI.
class ThumbnailGenerator {
public:
    explicit ThumbnailGenerator(const CodecRegistry& registry) : registry_(registry) {}
    Result<ImageBuffer> generate(const fs::path& path, int maxDimension, const CancellationToken& token) const;
    // Версия декодера, который будет использован (для ключа кэша).
    std::string decoderVersionFor(const fs::path& path) const;
private:
    const CodecRegistry& registry_;
};

} // namespace uic
