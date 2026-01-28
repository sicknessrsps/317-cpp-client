#pragma once
#include <list>
#include <unordered_map>
#include <memory>

namespace SDL_Client
{
    template<typename K, typename V>
    class LRUMap {
    public:
        explicit LRUMap(size_t capacity) : capacity_(capacity) {}

        std::shared_ptr<V> get(const K& key) {
            auto it = map_.find(key);
            if (it == map_.end()) return nullptr;
            cache_.splice(cache_.begin(), cache_, it->second);
            return it->second->second;
        }

        void clear() {
            cache_.clear();
            map_.clear();
        }

        void put(const K& key, std::shared_ptr<V> value) {
            auto it = map_.find(key);
            if (it != map_.end()) {
                cache_.erase(it->second);
                map_.erase(it);
            }
            cache_.emplace_front(key, value);
            map_[key] = cache_.begin();

            if (map_.size() > capacity_) {
                auto last = cache_.end();
                --last;
                map_.erase(last->first);
                cache_.pop_back();
            }
        }

    private:
        size_t capacity_;
        std::list<std::pair<K, std::shared_ptr<V>>> cache_;
        std::unordered_map<K, typename std::list<std::pair<K, std::shared_ptr<V>>>::iterator> map_;
    };

}