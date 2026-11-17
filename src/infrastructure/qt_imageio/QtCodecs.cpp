#include "QtCodecs.h"
#include "QtImageDecoder.h"
#include "QtImageEncoder.h"

namespace uic {

void registerQtCodecs(CodecRegistry& registry) {
    registry.addDecoder(std::make_shared<QtImageDecoder>());
    registry.addEncoder(std::make_shared<QtImageEncoder>());
}

} // namespace uic
