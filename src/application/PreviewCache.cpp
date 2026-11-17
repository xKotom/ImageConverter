#include "PreviewCache.h"
#include <chrono>

namespace uic {

std::string PreviewCache::makeKey(const fs::path& path, int maxDimension, const std::string& ver) {
    std::error_code ec;
    auto mtime = fs::last_write_time(path, ec).time_since_epoch().count();
    auto size = fs::file_size(path, ec);
    auto u8 = path.u8string();
    std::string key(reinterpret_cast<const char*>(u8.data()), u8.size());
    key += '|' + std::to_string(mtime) + '|' + std::to_string(size) + '|' +
           std::to_string(maxDimension) + '|' + ver;
    return key;
}

PreviewCache::Entry PreviewCache::get(const std::string& key) {
    std::lock_guard lock(m_);
    auto it = map_.find(key);
    if (it == map_.end()) return nullptr;
    lru_.splice(lru_.begin(), lru_, it->second.it);
    return it->second.value;
}

void PreviewCache::put(const std::string& key, Entry value) {
    if (!value) return;
    std::lock_guard lock(m_);
    if (value->byteSize() > maxBytes_) return; // не кэшируем то, что не помещается
    if (auto it = map_.find(key); it != map_.end()) {
        bytes_ -= it->second.value->byteSize();
        lru_.erase(it->second.it);
        map_.erase(it);
    }
    lru_.push_front(key);
    bytes_ += value->byteSize();
    map_[key] = Item{std::move(value), lru_.begin()};
    evict();
}

void PreviewCache::evict() {
    while (bytes_ > maxBytes_ && !lru_.empty()) {
        auto it = map_.find(lru_.back());
        bytes_ -= it->second.value->byteSize();
        map_.erase(it);
        lru_.pop_back();
    }
}

void PreviewCache::clear() {
    std::lock_guard lock(m_);
    map_.clear(); lru_.clear(); bytes_ = 0;
}

std::size_t PreviewCache::bytes() const { std::lock_guard lock(m_); return bytes_; }
std::size_t PreviewCache::count() const { std::lock_guard lock(m_); return map_.size(); }

} // namespace uic
