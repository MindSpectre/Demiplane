#pragma once

#include <memory>
#include <string_view>
#include <vector>

namespace demiplane::orm {

    // String interning pool for zero-allocation lookups
    class StringPool {
    private:
        struct StringNode {
            std::string data;
            std::string_view view;
            StringNode* next;
            
            StringNode(std::string str) : data(std::move(str)), view(data), next(nullptr) {}
        };
        
        std::vector<std::unique_ptr<StringNode>> buckets_;
        size_t bucket_count_;
        
        size_t hash(std::string_view sv) const noexcept {
            // FNV-1a hash
            size_t hash = 14695981039346656037ULL;
            for (char c : sv) {
                hash ^= static_cast<size_t>(c);
                hash *= 1099511628211ULL;
            }
            return hash % bucket_count_;
        }
        
    public:
        explicit StringPool(size_t initial_buckets = 1024) : bucket_count_(initial_buckets) {
            buckets_.resize(bucket_count_);
        }
        
        // Intern a string and return a stable string_view
        std::string_view intern(std::string str) {
            size_t bucket = hash(str);
            
            // Check if already exists
            StringNode* node = buckets_[bucket].get();
            while (node) {
                if (node->view == str) {
                    return node->view;
                }
                node = node->next;
            }
            
            // Create new node
            auto new_node = std::make_unique<StringNode>(std::move(str));
            std::string_view result = new_node->view;
            new_node->next = buckets_[bucket].release();
            buckets_[bucket] = std::move(new_node);
            
            // ReSharper disable once CppDFALocalValueEscapesFunction
            return result;
        }
        
        // Find existing string_view (zero allocation)
        std::string_view find(std::string_view sv) const {
            size_t bucket = hash(sv);
            StringNode* node = buckets_[bucket].get();
            
            while (node) {
                if (node->view == sv) {
                    return node->view;
                }
                node = node->next;
            }
            
            return {}; // Not found
        }
    };

    // Fast string map using interned strings
    template<typename Value>
    class FastStringMap {
    private:
        struct Entry {
            std::string_view key;
            Value value;
            bool occupied = false;
        };
        
        std::vector<Entry> entries_;
        size_t size_;
        size_t capacity_;
        
        size_t hash(std::string_view key) const noexcept {
            return std::hash<std::string_view>{}(key) % capacity_;
        }
        
        void resize() {
            std::vector<Entry> old_entries = std::move(entries_);
            capacity_ *= 2;
            entries_.clear();
            entries_.resize(capacity_);
            size_ = 0;
            
            for (const auto& entry : old_entries) {
                if (entry.occupied) {
                    insert_internal(entry.key, entry.value);
                }
            }
        }
        
        void insert_internal(std::string_view key, const Value& value) {
            size_t index = hash(key);
            
            while (entries_[index].occupied) {
                if (entries_[index].key == key) {
                    entries_[index].value = value;
                    return;
                }
                index = (index + 1) % capacity_;
            }
            
            entries_[index] = {key, value, true};
            ++size_;
        }
        
    public:
        explicit FastStringMap(size_t initial_capacity = 64) 
            : capacity_(initial_capacity), size_(0) {
            entries_.resize(capacity_);
        }
        
        void insert(std::string_view key, const Value& value) {
            if (size_ >= capacity_ * 0.75) {
                resize();
            }
            insert_internal(key, value);
        }
        
        Value* find(std::string_view key) {
            size_t index = hash(key);
            
            while (entries_[index].occupied) {
                if (entries_[index].key == key) {
                    return &entries_[index].value;
                }
                index = (index + 1) % capacity_;
            }
            
            return nullptr;
        }
        
        const Value* find(std::string_view key) const {
            return const_cast<FastStringMap*>(this)->find(key);
        }
    };

} // namespace demiplane::orm