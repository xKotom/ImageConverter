#pragma once
#include "domain/Models.h"

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace uic {

// Кэш превью, ограниченный по памяти (LRU). Ключ содержит путь, время изменения и размер файла,
// целевой размер и версию декодера — изменение файла автоматически инвалидирует запись.
class PreviewCache {
public:
    using Entry = std::shared_ptr<const ImageBuffer>;
    explicit PreviewCache(std::size_t maxBytes = 128u * 1024 * 1024) : maxBytes_(maxBytes) {}

    static std::string makeKey(const fs::path& path, int maxDimension, const std::string& decoderVersion);

    Entry get(const std::string& key);
    void put(const std::string& key, Entry value);
    void clear();
    std::size_t bytes() const;
    std::size_t count() const;

private:
    void evict();
    mutable std::mutex m_;
    std::size_t maxBytes_;
    std::size_t bytes_ = 0;
    std::list<std::string> lru_; // front = самый свежий
    struct Item { Entry value; std::list<std::string>::iterator it; };
    std::unordered_map<std::string, Item> map_;
};

} // namespace uic
