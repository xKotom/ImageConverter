#pragma once
#include "domain/Models.h"

#include <QStringList>
#include <optional>

namespace uic {

struct CommandLineOptions {
    std::vector<fs::path> paths;
    std::optional<ImageFormat> format;
    std::optional<int> quality;
    std::optional<fs::path> outputDir;
    bool autoStart = false;
    bool listCodecs = false;
    QString error; // непусто — неверные аргументы
};

// Разбор аргументов запуска. Позже сюда же придёт payload от Shell-команды Проводника.
class CommandLineHandler {
public:
    static CommandLineOptions parse(const QStringList& arguments);
};

} // namespace uic
