#include "QtCodecs.h"
#include "QtImageDecoder.h"
#include "QtImageEncoder.h"

namespace uic {

void registerQtCodecs(CodecRegistry& registry) {
    registry.addDecoder(std::make_shared<QtImageDecoder>());
    registry.addEncoder(std::make_shared<QtImageEncoder>());
}

std::string describeCapabilities(const CodecRegistry& registry) {
    auto join = [](const std::vector<ImageFormat>& v) {
        std::string s;
        for (auto f : v) { if (!s.empty()) s += ", "; s += traits(f).label; }
        return s.empty() ? std::string("—") : s;
    };
    return "Чтение: " + join(registry.readableFormats()) + "; запись: " + join(registry.writableFormats());
}

} // namespace uic
