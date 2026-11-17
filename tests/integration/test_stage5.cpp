// Этап 5: HEIC/HEIF, AVIF, GIF/TIFF, анимация и метаданные — на реальных кодеках.
#include "application/CodecRegistry.h"
#include "application/ConversionEngine.h"
#include "application/ConversionPlanner.h"
#include "application/FileAnalyzer.h"
#include "infrastructure/codecs/Codecs.h"
#include "infrastructure/codecs/GifEncoder.h"
#include "infrastructure/qt_imageio/QtConvert.h"

#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QtTest>
#include <exiv2/exiv2.hpp>
#include <fstream>
#include <libheif/heif.h>

using namespace uic;

namespace {

QImage makeImage(int w, int h, bool alpha = false) {
    QImage img(w, h, alpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, qRgba(x * 255 / w, y * 255 / h, (x * y) % 255, alpha && x < w / 4 ? 0 : 255));
    return img;
}

ImageSequence makeSequence(int frames, int w = 40, int h = 30, bool transparentCorner = false) {
    ImageSequence seq;
    for (int i = 0; i < frames; ++i) {
        QImage img(w, h, QImage::Format_ARGB32);
        const QColor c = QColor::fromHsv((i * 360) / frames, 255, 255);
        img.fill(c);
        if (transparentCorner)
            for (int y = 0; y < h / 3; ++y)
                for (int x = 0; x < w / 3; ++x) img.setPixel(x, y, qRgba(0, 0, 0, 0));
        seq.frames.push_back(toBuffer(img));
        seq.delaysMs.push_back(100 * (i + 1));
    }
    seq.loopCount = 0;
    return seq;
}

std::vector<std::uint8_t> readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void writeFile(const fs::path& p, const std::vector<std::uint8_t>& d) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
}

// JPEG с заданными EXIF (камера, дата, GPS, ориентация) — подготовка эталона.
fs::path makeJpegWithExif(const fs::path& path, bool orientationRotate = false) {
    QImageWriter wr(toQString(path), "jpeg");
    if (!wr.write(makeImage(64, 48))) qFatal("jpeg write failed");
    auto bytes = readFile(path);
    auto img = Exiv2::ImageFactory::open(bytes.data(), bytes.size());
    img->readMetadata();
    Exiv2::ExifData& e = img->exifData();
    e["Exif.Image.Make"] = "TestCam";
    e["Exif.Image.Model"] = "Model X";
    e["Exif.Photo.DateTimeOriginal"] = "2024:05:17 10:20:30";
    e["Exif.GPSInfo.GPSLatitudeRef"] = "N";
    e["Exif.GPSInfo.GPSLatitude"] = "55/1 45/1 0/1";
    e["Exif.GPSInfo.GPSLongitudeRef"] = "E";
    e["Exif.GPSInfo.GPSLongitude"] = "37/1 37/1 0/1";
    e["Exif.Image.Orientation"] = static_cast<uint16_t>(orientationRotate ? 6 : 1);
    img->writeMetadata();
    Exiv2::BasicIo& io = img->io();
    io.seek(0, Exiv2::BasicIo::beg);
    std::vector<std::uint8_t> out(static_cast<size_t>(io.size()));
    io.read(out.data(), out.size());
    writeFile(path, out);
    return path;
}

struct ExifProbe { bool exif = false, gps = false, make = false; int orientation = 0; bool iptcOrXmp = false; };
ExifProbe probeExif(const fs::path& p) {
    ExifProbe r;
    auto bytes = readFile(p);
    auto img = Exiv2::ImageFactory::open(bytes.data(), bytes.size());
    img->readMetadata();
    r.exif = !img->exifData().empty();
    for (const auto& d : img->exifData()) {
        if (d.groupName() == "GPSInfo") r.gps = true;
        if (d.key() == "Exif.Image.Make") r.make = true;
        if (d.key() == "Exif.Image.Orientation") r.orientation = static_cast<int>(d.toInt64());
    }
    return r;
}

} // namespace

class Stage5Tests : public QObject {
    Q_OBJECT
    CodecRegistry reg_;
    QTemporaryDir tmp_;
    fs::path root_;

    fs::path sub(const QString& name) {
        fs::path d = root_ / name.toStdU16String();
        fs::create_directories(d);
        return d;
    }
    FileInfo analyze(const fs::path& p) {
        auto r = FileAnalyzer(reg_).analyze(p);
        if (!r.ok()) qFatal("analyze failed: %s", r.error().message.c_str());
        return r.value();
    }
    ConversionResult convert(const FileInfo& info, const ConversionSettings& s) {
        return ConversionEngine(reg_).convert(info, s, CancellationToken(), nullptr);
    }
    static bool anyWarning(const ConversionResult& r, const QString& needle) {
        for (auto& w : r.warnings) if (QString::fromStdString(w).contains(needle)) return true;
        return false;
    }

private slots:
    void initTestCase() {
        QVERIFY(tmp_.isValid());
        root_ = fs::path(tmp_.path().toStdU16String());
        registerAllCodecs(reg_);
    }

    // ---------- реестр: реальная поддержка ----------
    void registry_reportsRealCapabilities() {
        for (auto f : {ImageFormat::Gif, ImageFormat::Webp, ImageFormat::Tiff}) {
            QVERIFY2(reg_.capability(f).readSequence, traits(f).name);
            QVERIFY2(reg_.capability(f).writeSequence, traits(f).name);
        }
        QVERIFY(!reg_.capability(ImageFormat::Jpeg).writeSequence);
        QVERIFY(reg_.capability(ImageFormat::Jpeg).writeMetadata);
        QVERIFY(!reg_.capability(ImageFormat::Bmp).writeMetadata);
        QVERIFY(reg_.capability(ImageFormat::Avif).write);
        QVERIFY(!reg_.capability(ImageFormat::Heif).write); // лицензия x265 (GPL)
    }

    // ---------- GIF / WEBP / TIFF: анимация ----------
    void gif_encodeThenAnalyze() {
        const fs::path dir = sub("gif");
        const fs::path f = dir / "anim.gif";
        GifEncoder enc;
        auto st = enc.encodeSequence(makeSequence(4, 40, 30, true), f, EncoderOptions{ImageFormat::Gif, 90}, CancellationToken(), nullptr);
        QVERIFY2(st.ok(), st.ok() ? "" : st.error().message.c_str());
        const FileInfo info = analyze(f);
        QCOMPARE(info.actualFormat, ImageFormat::Gif);
        QCOMPARE(info.frameCount, 4);
        QVERIFY(info.animated());
        QVERIFY(info.hasAlpha);
    }

    void animation_roundTripAllFrames() {
        const fs::path dir = sub("anim");
        const fs::path gif = dir / "a.gif";
        QVERIFY(GifEncoder().encodeSequence(makeSequence(5), gif, EncoderOptions{ImageFormat::Gif, 90}, CancellationToken(), nullptr).ok());

        FileInfo cur = analyze(gif);
        QCOMPARE(cur.frameCount, 5);
        // GIF -> WEBP -> TIFF -> GIF: число кадров должно сохраняться на каждом шаге
        for (ImageFormat target : {ImageFormat::Webp, ImageFormat::Tiff, ImageFormat::Gif}) {
            ConversionSettings s;
            s.targetFormat = target;
            s.outputDir = root_ / "anim_out" / traits(target).name;
            auto r = convert(cur, s);
            QVERIFY2(r.success, r.error ? r.error->message.c_str() : "");
            const FileInfo out = analyze(r.outputPath);
            QCOMPARE(out.actualFormat, target);
            QCOMPARE(out.frameCount, 5);
            QCOMPARE(out.width, 40);
            QCOMPARE(out.height, 30);
            cur = out;
        }
    }

    void animation_delaysSurviveGifToWebp() {
        const fs::path dir = sub("delays");
        QVERIFY(GifEncoder().encodeSequence(makeSequence(3), dir / "d.gif", EncoderOptions{ImageFormat::Gif, 90}, CancellationToken(), nullptr).ok());
        ConversionSettings s; s.targetFormat = ImageFormat::Webp;
        auto r = convert(analyze(dir / "d.gif"), s);
        QVERIFY(r.success);
        // читаем задержки из результата тем же декодером
        auto seq = reg_.sequenceDecoderFor(ImageFormat::Webp)->decodeSequence(r.outputPath, CancellationToken(), nullptr);
        QVERIFY(seq.ok());
        QCOMPARE(int(seq.value().frames.size()), 3);
        QCOMPARE(seq.value().delaysMs[0], 100);
        QCOMPARE(seq.value().delaysMs[1], 200);
        QCOMPARE(seq.value().delaysMs[2], 300);
    }

    void animation_policies() {
        const fs::path dir = sub("policy");
        QVERIFY(GifEncoder().encodeSequence(makeSequence(3), dir / "p.gif", EncoderOptions{ImageFormat::Gif, 90}, CancellationToken(), nullptr).ok());
        const FileInfo info = analyze(dir / "p.gif");
        ConversionPlanner planner(reg_);

        ConversionSettings s;
        s.targetFormat = ImageFormat::Webp;
        auto all = planner.plan(info, s);
        QVERIFY(all.allowed && all.allFrames);
        QCOMPARE(all.framesToWrite, 3);

        s.targetFormat = ImageFormat::Png; // PNG анимацию не пишет -> первый кадр с предупреждением
        auto first = planner.plan(info, s);
        QVERIFY(first.allowed && !first.allFrames);
        bool warned = false;
        for (auto& w : first.warnings) warned |= QString::fromStdString(w).contains("только первый кадр");
        QVERIFY(warned);
        auto r = convert(info, s);
        QVERIFY(r.success);
        QCOMPARE(analyze(r.outputPath).frameCount, 1);

        s.animation = AnimationPolicy::FirstFrameWithWarning;
        s.targetFormat = ImageFormat::Webp; // можно было бы все кадры, но пользователь просит один
        auto one = planner.plan(info, s);
        QVERIFY(one.allowed && !one.allFrames);

        s.animation = AnimationPolicy::Reject;
        QVERIFY(!planner.plan(info, s).allowed);
    }

    void gif_staticImageAndTransparency() {
        const fs::path dir = sub("gifstatic");
        QImageWriter wr(toQString(dir / "s.png"), "png");
        QVERIFY(wr.write(makeImage(64, 48, true)));
        ConversionSettings s; s.targetFormat = ImageFormat::Gif;
        auto r = convert(analyze(dir / "s.png"), s);
        QVERIFY2(r.success, r.error ? r.error->message.c_str() : "");
        QVERIFY(anyWarning(r, "256"));
        QImage out(toQString(r.outputPath));
        QVERIFY(!out.isNull());
        QCOMPARE(out.width(), 64);
        QVERIFY(qAlpha(out.pixel(0, 0)) == 0);     // прозрачная область осталась прозрачной
        QVERIFY(qAlpha(out.pixel(63, 47)) == 255);
    }

    void tiff_multipageToJpegWarnsFirstPage() {
        const fs::path dir = sub("tiffpages");
        ConversionSettings s; s.targetFormat = ImageFormat::Tiff;
        const fs::path gif = dir / "g.gif";
        QVERIFY(GifEncoder().encodeSequence(makeSequence(3), gif, EncoderOptions{ImageFormat::Gif, 90}, CancellationToken(), nullptr).ok());
        auto t = convert(analyze(gif), s);
        QVERIFY2(t.success, t.error ? t.error->message.c_str() : "");
        const FileInfo multi = analyze(t.outputPath);
        QCOMPARE(multi.frameCount, 3);

        s.targetFormat = ImageFormat::Jpeg;
        auto j = convert(multi, s);
        QVERIFY(j.success);
        QVERIFY(anyWarning(j, "первый кадр"));
        QCOMPARE(analyze(j.outputPath).frameCount, 1);
    }

    // ---------- HEIF / AVIF ----------
    void avif_roundTrip() {
        const fs::path dir = sub("avif");
        QImageWriter wr(toQString(dir / "a.png"), "png");
        QVERIFY(wr.write(makeImage(96, 64, true)));

        ConversionSettings s; s.targetFormat = ImageFormat::Avif; s.quality = 80;
        auto r = convert(analyze(dir / "a.png"), s);
        QVERIFY2(r.success, r.error ? r.error->message.c_str() : "");
        const FileInfo av = analyze(r.outputPath);
        QCOMPARE(av.actualFormat, ImageFormat::Avif);
        QCOMPARE(av.width, 96);
        QCOMPARE(av.height, 64);
        QVERIFY(av.hasAlpha);

        s.targetFormat = ImageFormat::Png;
        auto back = convert(av, s);
        QVERIFY2(back.success, back.error ? back.error->message.c_str() : "");
        QImage img(toQString(back.outputPath));
        QCOMPARE(img.width(), 96);
        QVERIFY(qAlpha(img.pixel(0, 0)) < 20);        // прозрачность сохранилась
        QVERIFY(qAlpha(img.pixel(95, 63)) > 235);
    }

    void avif_extensionMismatchAndPreview() {
        const fs::path dir = sub("avif2");
        QImageWriter(toQString(dir / "x.png"), "png").write(makeImage(80, 60));
        ConversionSettings s; s.targetFormat = ImageFormat::Avif;
        auto r = convert(analyze(dir / "x.png"), s);
        QVERIFY(r.success);
        const fs::path fake = dir / "photo.jpg"; // внутри AVIF
        fs::copy_file(r.outputPath, fake);
        const FileInfo info = analyze(fake);
        QCOMPARE(info.actualFormat, ImageFormat::Avif);
        QVERIFY(info.extensionMismatch());
        // scaled read для превью
        auto thumb = reg_.decoderFor(ImageFormat::Avif)->decode(fake, DecodeOptions{32, true}, CancellationToken(), nullptr);
        QVERIFY(thumb.ok());
        QVERIFY(thumb.value().width <= 32 && thumb.value().height <= 32);
    }

    void heic_decodeIfEncoderAvailable() {
        // Эталонный HEIC получаем напрямую через libheif (если в системе есть HEVC-кодировщик).
        heif_init(nullptr);
        heif_context* ctx = heif_context_alloc();
        heif_encoder* enc = nullptr;
        if (heif_context_get_encoder_for_format(ctx, heif_compression_HEVC, &enc).code != heif_error_Ok) {
            heif_context_free(ctx);
            QSKIP("В системе нет кодировщика HEVC: эталонный HEIC создать нечем");
        }
        heif_encoder_set_lossy_quality(enc, 90);
        heif_image* img = nullptr;
        QCOMPARE(heif_image_create(64, 48, heif_colorspace_RGB, heif_chroma_interleaved_RGB, &img).code, heif_error_Ok);
        heif_image_add_plane(img, heif_channel_interleaved, 64, 48, 8);
        int stride = 0;
        uint8_t* p = heif_image_get_plane(img, heif_channel_interleaved, &stride);
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 64; ++x) { p[y * stride + x * 3] = uint8_t(x * 4); p[y * stride + x * 3 + 1] = uint8_t(y * 5); p[y * stride + x * 3 + 2] = 128; }
        heif_image_handle* h = nullptr;
        QCOMPARE(heif_context_encode_image(ctx, img, enc, nullptr, &h).code, heif_error_Ok);
        const fs::path dir = sub("heic");
        const std::string file = (dir / "ref.heic").string();
        QCOMPARE(heif_context_write_to_file(ctx, file.c_str()).code, heif_error_Ok);
        heif_image_handle_release(h); heif_image_release(img); heif_encoder_release(enc); heif_context_free(ctx);

        const FileInfo info = analyze(dir / "ref.heic");
        QCOMPARE(info.actualFormat, ImageFormat::Heif);
        QCOMPARE(info.width, 64);
        QCOMPARE(info.height, 48);
        ConversionSettings s; s.targetFormat = ImageFormat::Jpeg;
        auto r = convert(info, s);
        QVERIFY2(r.success, r.error ? r.error->message.c_str() : "");
        QCOMPARE(analyze(r.outputPath).width, 64);
        s.targetFormat = ImageFormat::Png;
        QVERIFY(convert(info, s).success);
    }

    void heic_withoutDecoderIsExplained() {
        // Файл с сигнатурой HEIC, но без валидного содержимого: честная ошибка, никаких выходных файлов
        const fs::path dir = sub("heicbad");
        {
            std::ofstream f(dir / "fake.heic", std::ios::binary);
            f << std::string("\0\0\0\x18" "ftypheic\0\0\0\0mif1heic", 24) << std::string(64, '\0');
        }
        auto r = FileAnalyzer(reg_).analyze(dir / "fake.heic");
        QVERIFY(!r.ok());
        size_t n = 0; for (auto& e : fs::directory_iterator(dir)) { (void)e; ++n; }
        QCOMPARE(n, size_t(1));
    }

    // ---------- метаданные ----------
    void metadata_inspectAndPolicies() {
        const fs::path dir = sub("meta");
        const fs::path src = makeJpegWithExif(dir / "cam.jpg");
        const FileInfo info = analyze(src);
        QVERIFY(info.metadata.hasExif);
        QVERIFY(info.metadata.hasGps);

        struct Case { ImageFormat fmt; };
        for (ImageFormat fmt : {ImageFormat::Jpeg, ImageFormat::Png, ImageFormat::Webp, ImageFormat::Tiff}) {
            // 1. KeepWithoutGps (по умолчанию): камера есть, GPS нет
            ConversionSettings s; s.targetFormat = fmt;
            s.outputDir = root_ / "meta_out" / traits(fmt).name / "nogps";
            auto r = convert(info, s);
            QVERIFY2(r.success, r.error ? r.error->message.c_str() : "");
            ExifProbe p = probeExif(r.outputPath);
            QVERIFY2(p.exif && p.make, traits(fmt).name);
            QVERIFY2(!p.gps, traits(fmt).name);
            QVERIFY(p.orientation == 0 || p.orientation == 1);
            QVERIFY(anyWarning(r, "GPS-координаты съёмки будут удалены"));

            // 2. KeepAll: GPS на месте — только по явному выбору
            s.metadata = MetadataPolicy::KeepAll;
            s.outputDir = root_ / "meta_out" / traits(fmt).name / "all";
            r = convert(info, s);
            QVERIFY(r.success);
            p = probeExif(r.outputPath);
            QVERIFY2(p.gps && p.make, traits(fmt).name);
            QVERIFY(anyWarning(r, "ВНИМАНИЕ"));

            // 3. Strip: ничего нет
            s.metadata = MetadataPolicy::Strip;
            s.outputDir = root_ / "meta_out" / traits(fmt).name / "strip";
            r = convert(info, s);
            QVERIFY(r.success);
            p = probeExif(r.outputPath);
            QVERIFY2(!p.make && !p.gps, traits(fmt).name);
        }
    }

    void metadata_formatWithoutSupportWarns() {
        const fs::path dir = sub("meta2");
        const FileInfo info = analyze(makeJpegWithExif(dir / "c.jpg"));
        ConversionSettings s; s.targetFormat = ImageFormat::Bmp;
        auto r = convert(info, s);
        QVERIFY(r.success);
        QVERIFY(anyWarning(r, "будут потеряны"));
        s.targetFormat = ImageFormat::Avif;
        QVERIFY(anyWarning(convert(info, s), "будут потеряны"));
    }

    void metadata_orientationResetAfterRotation() {
        const fs::path dir = sub("meta3");
        const FileInfo info = analyze(makeJpegWithExif(dir / "r.jpg", true)); // ориентация 6
        QCOMPARE(info.exifOrientation, 6);
        QCOMPARE(info.width, 48); // с учётом ориентации 64x48 -> 48x64
        ConversionSettings s; s.targetFormat = ImageFormat::Jpeg;
        auto r = convert(info, s);
        QVERIFY2(r.success, r.error ? r.error->message.c_str() : "");
        QImage out(toQString(r.outputPath));
        QCOMPARE(out.width(), 48);   // применена один раз
        QCOMPARE(out.height(), 64);
        const ExifProbe p = probeExif(r.outputPath);
        QVERIFY(p.orientation == 0 || p.orientation == 1); // повторного поворота при просмотре не будет
    }

    // ---------- юникодные пути ----------
    void unicodePaths_everywhere() {
        const fs::path dir = sub(QString::fromUtf8("папка тест"));
        const fs::path src = dir / fs::path(u"фото №1.jpg");
        makeJpegWithExif(src);
        const FileInfo info = analyze(src);
        QVERIFY(info.metadata.hasExif);
        for (ImageFormat fmt : {ImageFormat::Gif, ImageFormat::Tiff, ImageFormat::Webp, ImageFormat::Avif,
                                ImageFormat::Png, ImageFormat::Jpeg, ImageFormat::Bmp}) {
            ConversionSettings s; s.targetFormat = fmt;
            s.outputDir = dir / fs::path(u"результат") / traits(fmt).name;
            s.metadata = MetadataPolicy::KeepWithoutGps;
            auto r = convert(info, s);
            QVERIFY2(r.success, qPrintable(QString("%1: %2").arg(traits(fmt).name, QString::fromStdString(r.error ? r.error->message : ""))));
            QVERIFY(fs::exists(r.outputPath));
            QCOMPARE(analyze(r.outputPath).width, 64);
        }
    }
};

QTEST_GUILESS_MAIN(Stage5Tests)
#include "test_stage5.moc"
