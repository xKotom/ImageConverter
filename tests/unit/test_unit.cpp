// Модульные тесты чистой логики: без GUI, без реальных кодеков (кроме AtomicFileWriter — файловая система).
#include "application/AtomicFileWriter.h"
#include "application/ConversionEngine.h"
#include "application/ConversionPlanner.h"
#include "application/FormatDetector.h"
#include "application/OutputNamer.h"
#include "application/PreviewCache.h"

#include <QTemporaryDir>
#include <QtTest>
#include <fstream>
#include <set>

using namespace uic;

namespace {
// Заглушки портов: тестируем планировщик без Qt Image I/O.
struct FakeDecoder : IImageDecoder {
    std::string id() const override { return "fake"; }
    std::string version() const override { return "1"; }
    bool supports(ImageFormat f) const override { return f != ImageFormat::Heif; }
    Result<ImageProbe> probe(const fs::path&) override { return ImageProbe{}; }
    Result<ImageBuffer> decode(const fs::path&, const DecodeOptions&, const CancellationToken&, const ProgressSink&) override {
        return makeError(ErrorCode::Internal, "not used");
    }
};
struct FakeEncoder : IImageEncoder {
    std::string id() const override { return "fake"; }
    bool supports(ImageFormat f) const override { return f == ImageFormat::Jpeg || f == ImageFormat::Png; }
    Status encode(const ImageBuffer&, const fs::path&, const EncoderOptions&, const CancellationToken&, const ProgressSink&) override {
        return Ok{};
    }
};
std::span<const std::uint8_t> bytes(const char* s, size_t n) {
    return {reinterpret_cast<const std::uint8_t*>(s), n};
}
} // namespace

class UnitTests : public QObject {
    Q_OBJECT
private slots:
    void detector_recognizesBySignature() {
        QCOMPARE(FormatDetector::detect(bytes("\xFF\xD8\xFF\xE0xxxxxxxxxxxx", 16)), ImageFormat::Jpeg);
        QCOMPARE(FormatDetector::detect(bytes("\x89PNG\r\n\x1A\nxxxxxxxx", 16)), ImageFormat::Png);
        QCOMPARE(FormatDetector::detect(bytes("RIFF\x10\0\0\0WEBPVP8 ", 16)), ImageFormat::Webp);
        QCOMPARE(FormatDetector::detect(bytes("GIF89a\x01\0\x01\0xxxxxxxx", 16)), ImageFormat::Gif);
        QCOMPARE(FormatDetector::detect(bytes("II*\0\x08\0\0\0xxxxxxxx", 16)), ImageFormat::Tiff);
        QCOMPARE(FormatDetector::detect(bytes("\0\0\0\x18" "ftypheic" "xxxxxxxxxxxx", 20)), ImageFormat::Heif);
        QCOMPARE(FormatDetector::detect(bytes("\0\0\0\x18" "ftypavif" "xxxxxxxxxxxx", 20)), ImageFormat::Avif);
        QCOMPARE(FormatDetector::detect(bytes("random text content here", 24)), ImageFormat::Unknown);
        QCOMPARE(FormatDetector::detect({}), ImageFormat::Unknown);
    }

    void detector_ignoresExtension() {
        QTemporaryDir dir;
        const fs::path p = fs::path(dir.path().toStdU16String()) / "photo.jpg"; // а внутри PNG
        std::ofstream(p, std::ios::binary) << "\x89PNG\r\n\x1A\n" << std::string(32, 'x');
        auto r = FormatDetector::detectFile(p);
        QVERIFY(r.ok());
        QCOMPARE(r.value(), ImageFormat::Png);
    }

    void format_extensionsAndNames() {
        QCOMPARE(formatFromExtension(".JPG"), ImageFormat::Jpeg);
        QCOMPARE(formatFromExtension("tif"), ImageFormat::Tiff);
        QCOMPARE(formatFromName("jpeg"), ImageFormat::Jpeg);
        QCOMPARE(formatFromExtension("xyz"), ImageFormat::Unknown);
        QCOMPARE(QString::fromStdString(primaryExtension(ImageFormat::Jpeg)), QString("jpg"));
    }

    void fileInfo_extensionMismatch() {
        FileInfo i;
        i.actualFormat = ImageFormat::Png;
        i.extensionHint = "jpg";
        QVERIFY(i.extensionMismatch());
        i.extensionHint = "png";
        QVERIFY(!i.extensionMismatch());
        i.extensionHint = "dat"; // неизвестное расширение — не считается несовпадением
        QVERIFY(!i.extensionMismatch());
    }

    void namer_suffixOnConflict() {
        std::set<fs::path> existing{fs::path("out") / "a.png", fs::path("out") / "a (1).png"};
        auto ex = [&](const fs::path& p) { return existing.count(p) > 0; };
        auto d = OutputNamer::resolve("in/a.jpg", "out", ImageFormat::Png, ConflictPolicy::Suffix, ex);
        QCOMPARE(d.path, fs::path("out") / "a (2).png");
        QVERIFY(!d.skip && !d.replace);
    }

    void namer_skipAndReplace() {
        auto ex = [](const fs::path&) { return true; };
        auto s = OutputNamer::resolve("in/a.jpg", "out", ImageFormat::Png, ConflictPolicy::Skip, ex);
        QVERIFY(s.skip);
        auto r = OutputNamer::resolve("in/a.jpg", "out", ImageFormat::Png, ConflictPolicy::Replace, ex);
        QVERIFY(r.replace);
        QCOMPARE(r.path, fs::path("out") / "a.png");
    }

    void namer_neverTargetsSource() {
        QTemporaryDir dir;
        const fs::path d = fs::path(dir.path().toStdU16String());
        const fs::path src = d / "a.png";
        std::ofstream(src) << "x";
        // PNG -> PNG в той же папке при политике Replace: исходник трогать нельзя.
        auto r = OutputNamer::resolve(src, {}, ImageFormat::Png, ConflictPolicy::Replace);
        QVERIFY(!r.replace);
        QCOMPARE(r.path, d / "a (1).png");
        auto s = OutputNamer::resolve(src, {}, ImageFormat::Png, ConflictPolicy::Skip);
        QVERIFY(!s.skip);
        QCOMPARE(s.path, d / "a (1).png");
    }

    void namer_unicodeNames() {
        auto d = OutputNamer::resolve(fs::path(u"in/фото 1.jpg"), "out", ImageFormat::Webp, ConflictPolicy::Suffix,
                                      [](const fs::path&) { return false; });
        QCOMPARE(d.path, fs::path("out") / fs::path(u"фото 1.webp"));
    }

    void atomicWriter_commitAndRollback() {
        QTemporaryDir dir;
        const fs::path d = fs::path(dir.path().toStdU16String());
        fs::path tmp;
        {
            auto w = AtomicFileWriter::begin(d);
            QVERIFY(w.ok());
            tmp = w.value().tempPath();
            QVERIFY(fs::exists(tmp));
        } // без commit() деструктор обязан удалить временный файл
        QVERIFY(!fs::exists(tmp));

        auto w = AtomicFileWriter::begin(d);
        QVERIFY(w.ok());
        std::ofstream(w.value().tempPath(), std::ios::binary) << "data";
        QVERIFY(w.value().commit(d / "result.bin", false).ok());
        QVERIFY(fs::exists(d / "result.bin"));
        QVERIFY(!fs::exists(w.value().tempPath()) || w.value().tempPath().empty());
    }

    void atomicWriter_refusesSilentOverwrite() {
        QTemporaryDir dir;
        const fs::path d = fs::path(dir.path().toStdU16String());
        std::ofstream(d / "x.bin") << "old";
        auto w = AtomicFileWriter::begin(d);
        QVERIFY(w.ok());
        std::ofstream(w.value().tempPath(), std::ios::binary) << "new";
        auto st = w.value().commit(d / "x.bin", false);
        QVERIFY(!st.ok());
        QCOMPARE(st.error().code, ErrorCode::AlreadyExists);
        // а с явным разрешением — заменяет
        QVERIFY(w.value().commit(d / "x.bin", true).ok());
        std::ifstream in(d / "x.bin");
        std::string s; in >> s;
        QCOMPARE(QString::fromStdString(s), QString("new"));
    }

    void planner_warningsAndRejections() {
        CodecRegistry reg;
        reg.addDecoder(std::make_shared<FakeDecoder>());
        reg.addEncoder(std::make_shared<FakeEncoder>());
        ConversionPlanner planner(reg);

        FileInfo info;
        info.path = "a.png";
        info.actualFormat = ImageFormat::Png;
        info.extensionHint = "jpg";
        info.width = 100; info.height = 100;
        info.hasAlpha = true;

        ConversionSettings s;
        s.targetFormat = ImageFormat::Jpeg;
        auto p = planner.plan(info, s);
        QVERIFY(p.allowed);
        const QString all = QString::fromStdString([&] { std::string r; for (auto& w : p.warnings) r += w + "\n"; return r; }());
        QVERIFY2(all.contains("Прозрачность"), qPrintable(all));
        QVERIFY2(all.contains("не соответствует содержимому"), qPrintable(all));
        QVERIFY2(all.contains("с потерями"), qPrintable(all));

        s.targetFormat = ImageFormat::Webp; // кодировщика нет
        QVERIFY(!planner.plan(info, s).allowed);

        info.actualFormat = ImageFormat::Heif; // декодера нет
        s.targetFormat = ImageFormat::Png;
        QVERIFY(!planner.plan(info, s).allowed);
    }

    void planner_animationPolicy() {
        CodecRegistry reg;
        reg.addDecoder(std::make_shared<FakeDecoder>());
        reg.addEncoder(std::make_shared<FakeEncoder>());
        ConversionPlanner planner(reg);
        FileInfo info;
        info.actualFormat = ImageFormat::Gif; info.extensionHint = "gif";
        info.width = 10; info.height = 10; info.frameCount = 5;
        ConversionSettings s;
        s.targetFormat = ImageFormat::Png;

        auto warn = planner.plan(info, s);
        QVERIFY(warn.allowed);
        QVERIFY(QString::fromStdString(warn.warnings.front()).contains("первый кадр"));

        s.animation = AnimationPolicy::Reject;
        QVERIFY(!planner.plan(info, s).allowed);
    }

    void planner_rejectsHugeImages() {
        CodecRegistry reg;
        reg.addDecoder(std::make_shared<FakeDecoder>());
        reg.addEncoder(std::make_shared<FakeEncoder>());
        FileInfo info;
        info.actualFormat = ImageFormat::Png;
        info.width = 20000; info.height = 20000; // 400 Мпикс
        ConversionSettings s;
        QVERIFY(!ConversionPlanner(reg).plan(info, s).allowed);
    }

    void engine_flattenAlpha() {
        ImageBuffer b;
        b.width = 3; b.height = 1; b.hasAlpha = true;
        b.rgba = {255, 0, 0, 255,   // непрозрачный красный
                  0, 0, 255, 0,     // полностью прозрачный -> фон
                  0, 0, 0, 128};    // полупрозрачный чёрный на белом -> серый
        ConversionEngine::flattenAlpha(b, 0xFFFFFF);
        QVERIFY(!b.hasAlpha);
        QCOMPARE(int(b.rgba[0]), 255); QCOMPARE(int(b.rgba[1]), 0); QCOMPARE(int(b.rgba[3]), 255);
        QCOMPARE(int(b.rgba[4]), 255); QCOMPARE(int(b.rgba[5]), 255); QCOMPARE(int(b.rgba[6]), 255);
        QVERIFY(std::abs(int(b.rgba[8]) - 127) <= 1);
    }

    void cache_lruEvictionAndInvalidation() {
        PreviewCache cache(1000);
        auto mk = [](size_t n) {
            auto b = std::make_shared<ImageBuffer>();
            b->width = 1; b->height = 1; b->rgba.assign(n, 0);
            return std::shared_ptr<const ImageBuffer>(b);
        };
        cache.put("a", mk(400));
        cache.put("b", mk(400));
        QVERIFY(cache.get("a")); // «a» становится самым свежим
        cache.put("c", mk(400)); // вытесняется «b»
        QVERIFY(cache.get("a"));
        QVERIFY(!cache.get("b"));
        QVERIFY(cache.get("c"));
        QVERIFY(cache.bytes() <= 1000);
        cache.put("huge", mk(5000)); // не помещается — не кэшируется
        QVERIFY(!cache.get("huge"));

        // ключ меняется при изменении файла
        QTemporaryDir dir;
        const fs::path f = fs::path(dir.path().toStdU16String()) / "p.bin";
        std::ofstream(f) << "1";
        const auto k1 = PreviewCache::makeKey(f, 640, "v1");
        std::ofstream(f, std::ios::app) << "22";
        QVERIFY(k1 != PreviewCache::makeKey(f, 640, "v1"));
        QVERIFY(PreviewCache::makeKey(f, 640, "v1") != PreviewCache::makeKey(f, 640, "v2"));
        QVERIFY(PreviewCache::makeKey(f, 640, "v1") != PreviewCache::makeKey(f, 320, "v1"));
    }
};

QTEST_GUILESS_MAIN(UnitTests)
#include "test_unit.moc"
