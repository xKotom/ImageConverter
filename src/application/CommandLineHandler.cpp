#include "CommandLineHandler.h"
#include <QString>

#include <QCommandLineParser>

namespace uic {

CommandLineOptions CommandLineHandler::parse(const QStringList& arguments) {
    CommandLineOptions o;
    QCommandLineParser p;
    p.addPositionalArgument("files", "Файлы или папки для добавления в очередь", "[files...]");
    p.addOption({{"f", "format"}, "Целевой формат (jpeg, png, bmp, webp, ...)", "format"});
    p.addOption({{"q", "quality"}, "Качество 1..100", "quality"});
    p.addOption({{"o", "output"}, "Папка результата", "dir"});
    p.addOption({"start", "Сразу запустить конвертацию"});
    p.addOption({"list-codecs", "Показать доступные форматы и выйти"});

    if (!p.parse(arguments)) { o.error = p.errorText(); return o; }
    for (const auto& a : p.positionalArguments()) o.paths.push_back(fs::path(a.toStdU16String()));
    if (p.isSet("format")) {
        auto f = formatFromName(p.value("format").toStdString());
        if (f == ImageFormat::Unknown) { o.error = "Неизвестный формат: " + p.value("format"); return o; }
        o.format = f;
    }
    if (p.isSet("quality")) {
        bool ok = false;
        int q = p.value("quality").toInt(&ok);
        if (!ok || q < 1 || q > 100) { o.error = "Качество должно быть числом 1..100"; return o; }
        o.quality = q;
    }
    if (p.isSet("output")) o.outputDir = fs::path(p.value("output").toStdU16String());
    o.autoStart = p.isSet("start");
    o.listCodecs = p.isSet("list-codecs");
    return o;
}

} // namespace uic
