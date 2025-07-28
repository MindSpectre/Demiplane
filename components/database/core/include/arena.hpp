// TODO: Suggest move common arena to mem tools

#pragma once
#include <gears_class_traits.hpp>
#include <memory>

namespace demiplane::db {
    class Arena : public gears::NonCopyable {
    public:
        explicit Arena(const std::size_t default_block_size = 64 * 1024)
            : head_(nullptr),
              default_block_size_(default_block_size),
              total_allocated_(0) {}

        ~Arena() {
            clear();
        }

        template <typename T>
        T* allocate(const std::size_t count = 1) {
            const std::size_t size         = sizeof(T) * count;
            const std::size_t aligned_size = (size + alignof(T) - 1) & ~(alignof(T) - 1);

            if (!head_ || head_->used + aligned_size > head_->size) {
                allocate_new_block(std::max(aligned_size, default_block_size_));
            }

            auto result = reinterpret_cast<T*>(head_->memory.get() + head_->used);
            head_->used += aligned_size;
            total_allocated_ += aligned_size;

            return result;
        }

        void clear() {
            while (head_) {
                Block* next = head_->next;
                delete head_;
                head_ = next;
            }
            total_allocated_ = 0;
        }

        [[nodiscard]] std::size_t total_allocated() const {
            return total_allocated_;
        }

    private:
        struct Block {
            std::unique_ptr<uint8_t[]> memory;
            std::size_t size;
            std::size_t used;
            Block* next;

            explicit Block(const std::size_t block_size)
                : memory(std::make_unique<uint8_t[]>(block_size)),
                  size(block_size),
                  used(0),
                  next(nullptr) {}
        };

        void allocate_new_block(const std::size_t size) {
            const auto new_block = new Block(size);
            new_block->next  = head_;
            head_            = new_block;
        }

        Block* head_;
        std::size_t default_block_size_;
        std::size_t total_allocated_;
    };
}
