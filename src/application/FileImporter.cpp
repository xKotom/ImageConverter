#include "FileImporter.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace uic {

std::string FileImporter::pathKey(const fs::path& p) {
    std::error_code ec;
    fs::path n = fs::absolute(p, ec).lexically_normal();
    auto u8 = n.u8string();
    std::string s(reinterpret_cast<const char*>(u8.data()), u8.size());
#ifdef _WIN32
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return c < 128 ? std::tolower(c) : c; });
#endif
    return s;
}

std::vector<fs::path> FileImporter::collect(const std::vector<fs::path>& inputs) {
    std::vector<fs::path> out;
    std::set<std::string> seen;
    auto add = [&](const fs::path& p) {
        fs::path n = p.lexically_normal();
        if (seen.insert(pathKey(n)).second) out.push_back(n);
    };
    for (const auto& in : inputs) {
        std::error_code ec;
        if (fs::is_directory(in, ec)) {
            fs::recursive_directory_iterator it(in, fs::directory_options::skip_permission_denied, ec), end;
            for (; !ec && it != end; it.increment(ec)) {
                std::error_code e2;
                if (it->is_regular_file(e2) &&
                    formatFromExtension(it->path().extension().string()) != ImageFormat::Unknown)
                    add(it->path());
            }
        } else {
            add(in);
        }
    }
    return out;
}

} // namespace uic
