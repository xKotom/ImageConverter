#include "Codecs.h"
#include "GifEncoder.h"
#include "HeifCodec.h"
#include "TiffSequenceEncoder.h"
#include "WebpAnimEncoder.h"
#include "infrastructure/metadata/Exiv2MetadataService.h"
#include "infrastructure/qt_imageio/QtCodecs.h"

namespace uic {

void registerAllCodecs(CodecRegistry& registry) {
    registerQtCodecs(registry);                       // первым: одиночные кадры идут через Qt
    registry.addDecoder(std::make_shared<HeifDecoder>());
    registry.addEncoder(std::make_shared<HeifEncoder>());
    registry.addEncoder(std::make_shared<GifEncoder>());
    registry.addEncoder(std::make_shared<WebpAnimEncoder>());
    registry.addEncoder(std::make_shared<TiffSequenceEncoder>());
    registry.setMetadataService(std::make_shared<Exiv2MetadataService>());
}

} // namespace uic
