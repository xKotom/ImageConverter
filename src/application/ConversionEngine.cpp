#include "ConversionEngine.h"
#include "AtomicFileWriter.h"
#include "ConversionPlanner.h"
#include "OutputNamer.h"

#include <chrono>

namespace uic {

void ConversionEngine::flattenAlpha(ImageBuffer& img, std::uint32_t bg) {
    const unsigned br = (bg >> 16) & 0xFF, bgc = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    std::uint8_t* p = img.rgba.data();
    const std::size_t n = static_cast<std::size_t>(img.width) * img.height;
    for (std::size_t i = 0; i < n; ++i, p += 4) {
        const unsigned a = p[3];
        p[0] = static_cast<std::uint8_t>((p[0] * a + br * (255 - a) + 127) / 255);
        p[1] = static_cast<std::uint8_t>((p[1] * a + bgc * (255 - a) + 127) / 255);
        p[2] = static_cast<std::uint8_t>((p[2] * a + bb * (255 - a) + 127) / 255);
        p[3] = 255;
    }
    img.hasAlpha = false;
}

ConversionResult ConversionEngine::convert(const FileInfo& info, const ConversionSettings& s,
                                           const CancellationToken& token, const ProgressSink& progress) const {
    const auto t0 = std::chrono::steady_clock::now();
    ConversionResult r;
    try {
        r = run(info, s, token, progress);
    } catch (const std::exception& e) {
        r = ConversionResult{};
        r.error = makeError(ErrorCode::Internal, std::string("Внутренняя ошибка: ") + e.what());
    } catch (...) {
        r = ConversionResult{};
        r.error = makeError(ErrorCode::Internal, "Неизвестная внутренняя ошибка");
    }
    r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - t0).count();
    return r;
}

ConversionResult ConversionEngine::run(const FileInfo& info, const ConversionSettings& s,
                                       const CancellationToken& token, const ProgressSink& progress) const {
    ConversionResult res;
    auto report = [&](int p) { if (progress) progress(p); };
    auto fail = [&](ErrorCode c, std::string m) {
        res.success = false;
        res.error = makeError(c, std::move(m));
        return res;
    };
    auto cancelled = [&] { return fail(ErrorCode::Cancelled, "Операция отменена"); };

    // 1. План (повторно: настройки и кодеки могли измениться с момента добавления файла)
    ConversionPlan plan = ConversionPlanner(registry_).plan(info, s);
    if (!plan.allowed) return fail(ErrorCode::UnsupportedFormat, plan.rejectReason);
    res.warnings = plan.warnings;
    if (token.cancelled()) return cancelled();

    IImageDecoder* dec = registry_.decoderFor(info.actualFormat);
    IImageEncoder* enc = registry_.encoderFor(s.targetFormat);
    if (!dec) return fail(ErrorCode::NoDecoder, "Декодер недоступен");
    if (!enc) return fail(ErrorCode::NoEncoder, "Кодировщик недоступен");

    // 2. Имя результата до тяжёлой работы: при Skip не тратим время
    NameDecision name = OutputNamer::resolve(info.path, s.outputDir, s.targetFormat, s.conflict);
    if (name.skip) {
        res.success = true;
        res.skipped = true;
        res.outputPath = name.path;
        res.warnings.push_back("Пропущено: файл с таким именем уже существует");
        report(100);
        return res;
    }
    report(5);

    const bool dropAlpha = !traits(s.targetFormat).supportsAlpha;
    const bool dropIcc = !traits(s.targetFormat).supportsIcc;
    auto prepare = [&](ImageBuffer& b) {
        if (b.hasAlpha && dropAlpha) flattenAlpha(b, s.backgroundRgb);
        if (dropIcc) b.iccProfile.clear();
    };

    // 3. Временный файл в целевой папке (тот же том, непредсказуемое имя)
    auto writer = AtomicFileWriter::begin(name.path.parent_path());
    if (!writer) return fail(writer.error().code, writer.error().message);
    const EncoderOptions eo{s.targetFormat, s.quality};

    int expectedW = 0, expectedH = 0, expectedFrames = 1;
    if (plan.allFrames) {
        // 4а. Все кадры: анимация / многостраничный файл
        IImageDecoder* sdec = registry_.sequenceDecoderFor(info.actualFormat);
        IImageEncoder* senc = registry_.sequenceEncoderFor(s.targetFormat);
        if (!sdec || !senc) return fail(ErrorCode::Internal, "Нет адаптеров для последовательности кадров");
        auto seqR = sdec->decodeSequence(info.path, token, nullptr);
        if (!seqR) return fail(seqR.error().code, seqR.error().message);
        ImageSequence seq = std::move(seqR.value());
        report(35);
        if (token.cancelled()) return cancelled();
        for (auto& f : seq.frames) prepare(f);
        expectedW = seq.frames.front().width;
        expectedH = seq.frames.front().height;
        expectedFrames = static_cast<int>(seq.frames.size());
        if (expectedFrames != info.frameCount)
            res.warnings.push_back("Число прочитанных кадров (" + std::to_string(expectedFrames) +
                                   ") отличается от заявленного (" + std::to_string(info.frameCount) + ")");
        auto st = senc->encodeSequence(seq, writer.value().tempPath(), eo, token, nullptr);
        if (!st) return fail(st.error().code, st.error().message); // writer удалит temp
    } else {
        // 4б. Один кадр (у анимации — первый, об этом предупреждено в плане)
        auto decoded = dec->decode(info.path, DecodeOptions{0, true}, token, nullptr);
        if (!decoded) return fail(decoded.error().code, decoded.error().message);
        ImageBuffer img = std::move(decoded.value());
        report(40);
        if (token.cancelled()) return cancelled();
        prepare(img);
        expectedW = img.width;
        expectedH = img.height;
        auto st = enc->encode(img, writer.value().tempPath(), eo, token, nullptr);
        if (!st) return fail(st.error().code, st.error().message);
    }
    report(75);
    if (token.cancelled()) return cancelled();

    // 5. Метаданные: сбой не губит результат, но о нём сообщается
    if (plan.transferMetadata) {
        auto mt = registry_.metadataService()->transfer(info.path, writer.value().tempPath(), s.metadata);
        if (!mt) res.warnings.push_back("Метаданные перенести не удалось: " + mt.error().message);
    }
    report(85);

    // 6. Проверка: размер > 0 и повторное чтение результата декодером целевого формата
    std::error_code ec;
    auto size = fs::file_size(writer.value().tempPath(), ec);
    if (ec || size == 0) return fail(ErrorCode::VerifyFailed, "Результат пустой или недоступен");
    if (IImageDecoder* vdec = registry_.decoderFor(s.targetFormat)) {
        auto check = vdec->probe(writer.value().tempPath());
        if (!check) return fail(ErrorCode::VerifyFailed, "Результат не читается: " + check.error().message);
        if (check.value().width != expectedW || check.value().height != expectedH)
            return fail(ErrorCode::VerifyFailed, "Размеры результата не совпадают с ожидаемыми");
        if (plan.allFrames && check.value().frameCount != expectedFrames)
            return fail(ErrorCode::VerifyFailed, "Число кадров в результате (" + std::to_string(check.value().frameCount) +
                                                     ") не совпадает с ожидаемым (" + std::to_string(expectedFrames) + ")");
    } else {
        res.warnings.push_back("Результат не проверен повторным чтением: декодера для этого формата нет");
    }
    report(92);
    if (token.cancelled()) return cancelled();

    // 7. Фиксация итогового имени
    auto cm = writer.value().commit(name.path, name.replace);
    if (!cm) return fail(cm.error().code, cm.error().message);

    res.success = true;
    res.outputPath = name.path;
    res.outputSize = size;
    report(100);
    return res;
}

} // namespace uic
