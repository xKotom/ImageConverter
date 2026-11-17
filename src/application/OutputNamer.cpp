#include "OutputNamer.h"

namespace uic {

NameDecision OutputNamer::resolve(const fs::path& source, const fs::path& outputDir,
                                  ImageFormat target, ConflictPolicy policy, const ExistsFn& existsFn) {
    auto exists = [&](const fs::path& p) {
        if (existsFn) return existsFn(p);
        std::error_code ec;
        return fs::exists(p, ec);
    };
    auto isSource = [&](const fs::path& p) {
        std::error_code ec;
        if (fs::equivalent(p, source, ec)) return true;
        return p.lexically_normal() == source.lexically_normal();
    };

    const fs::path dir = outputDir.empty() ? source.parent_path() : outputDir;
    const std::u8string stem = source.stem().u8string();
    const std::string ext = primaryExtension(target);
    auto make = [&](int n) {
        std::u8string name = stem;
        if (n > 0) {
            const std::string num = std::to_string(n);
            name += u8" (" + std::u8string(num.begin(), num.end()) + u8")";
        }
        name += u8".";
        name += std::u8string(ext.begin(), ext.end());
        return dir / fs::path(name);
    };

    fs::path first = make(0);
    const bool clash = exists(first);
    const bool selfClash = clash && isSource(first);

    NameDecision d;
    if (!clash) { d.path = first; return d; }

    if (policy == ConflictPolicy::Skip && !selfClash) { d.path = first; d.skip = true; return d; }
    if (policy == ConflictPolicy::Replace && !selfClash) { d.path = first; d.replace = true; return d; }

    // Suffix (и защита исходника при любой политике)
    for (int n = 1; n < 100000; ++n) {
        fs::path cand = make(n);
        if (!exists(cand)) { d.path = cand; return d; }
    }
    d.path = make(99999);
    return d;
}

} // namespace uic
