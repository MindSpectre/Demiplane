#pragma once
#include <algorithm>
#include <deque>
#include <functional>
#include <vector>

namespace demiplane::algorithms {
    // Default comparator that uses T::comp if available, otherwise operator<
    template <typename T>
    struct DefaultComparator {
        bool operator()(const T& a, const T& b) const {
            if constexpr (requires { T::comp(a, b); }) {
                return T::comp(a, b);
            } else {
                return a < b;
            }
        }
    };

    template <typename T>
    struct SlidingWindowConfig {
        std::size_t window_size                            = 1024;  // Size of sliding window
        std::size_t batch_size                             = 512;   // Batch size for processing
        bool enable_sorting                                = true;  // Enable/disable sorting
        std::function<bool(const T&, const T&)> comparator = DefaultComparator<T>{};

        // Performance tuning
        bool use_inplace_merge      = true;  // Use in-place merge for better memory usage
        std::size_t merge_threshold = 64;    // Switch to insertion sort for small sequences
    };


    template <typename T>
    class SlidingWindowSorter {
        public:
        using value_type        = T;
        using comparator_type   = std::function<bool(const T&, const T&)>;
        using consumer_function = std::function<void(const std::vector<T>&)>;

        private:
        SlidingWindowConfig<T> config_;
        std::vector<T> sorted_window_;
        std::vector<T> new_entries_;
        std::vector<T> merge_buffer_;
        consumer_function consumer_;

        // Statistics
        std::size_t total_processed_  = 0;
        std::size_t merge_operations_ = 0;
        std::size_t sort_operations_  = 0;

        public:
        explicit SlidingWindowSorter(SlidingWindowConfig<T> config, consumer_function consumer)
            : config_(std::move(config)),
              consumer_(std::move(consumer)) {
            sorted_window_.reserve(config_.window_size);
            new_entries_.reserve(config_.batch_size);
            merge_buffer_.reserve(config_.window_size + config_.batch_size);
        }

        // Add entries to be processed
        void add_entries(std::vector<T> entries) {
            if (entries.empty())
                return;

            // Move entries to new_entries buffer
            new_entries_.insert(
                new_entries_.end(), std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()));

            // Process if we have enough entries
            if (should_process()) {
                process_batch();
            }
        }

        // Add single entry
        void add_entry(T entry) {
            new_entries_.emplace_back(std::move(entry));

            if (should_process()) {
                process_batch();
            }
        }

        // Force processing of all remaining entries
        void flush() {
            if (!new_entries_.empty() || !sorted_window_.empty()) {
                process_batch(true /* force_all */);
            }
        }

        // Get statistics
        struct Statistics {
            std::size_t total_processed;
            std::size_t merge_operations;
            std::size_t sort_operations;
            double avg_merge_efficiency;
        };

        Statistics get_statistics() const {
            return {total_processed_,
                    merge_operations_,
                    sort_operations_,
                    sort_operations_ > 0
                        ? static_cast<double>(merge_operations_) / static_cast<double>(sort_operations_)
                        : 0.0};
        }

        // Reconfigure on the fly
        void reconfigure(SlidingWindowConfig<T> new_config) {
            flush();  // Process any pending entries with old config
            config_ = std::move(new_config);

            // Resize buffers if needed
            sorted_window_.reserve(config_.window_size);
            new_entries_.reserve(config_.batch_size);
            merge_buffer_.reserve(config_.window_size + config_.batch_size);
        }

        private:
        [[nodiscard]] bool should_process() const {
            return new_entries_.size() >= config_.batch_size;
        }

        void process_batch(const bool force_all = false) {
            if (new_entries_.empty() && sorted_window_.empty())
                return;

            if (config_.enable_sorting) {
                sort_and_merge();
            } else {
                // No sorting - just append new entries to window
                sorted_window_.insert(sorted_window_.end(),
                                      std::make_move_iterator(new_entries_.begin()),
                                      std::make_move_iterator(new_entries_.end()));
                new_entries_.clear();
            }

            // Determine how many entries to output
            std::size_t output_count;
            if (force_all) {
                output_count = sorted_window_.size();
            } else {
                output_count = calculate_output_count();
            }

            if (output_count > 0) {
                output_entries(output_count);
            }

            // Maintain window size
            maintain_window_size();
        }

        void sort_and_merge() {
            if (new_entries_.empty())
                return;

            // Sort new entries
            if (new_entries_.size() <= config_.merge_threshold) {
                insertion_sort(new_entries_.begin(), new_entries_.end(), config_.comparator);
            } else {
                std::sort(new_entries_.begin(), new_entries_.end(), config_.comparator);
            }
            sort_operations_++;

            if (sorted_window_.empty()) {
                // First batch - just move
                sorted_window_ = std::move(new_entries_);
                new_entries_.clear();
            } else {
                // Merge with existing sorted window
                merge_with_window();
                merge_operations_++;
            }
        }

        void merge_with_window() {
            if (config_.use_inplace_merge && can_use_inplace_merge()) {
                // In-place merge (memory efficient)
                perform_inplace_merge();
            } else {
                // Standard merge (faster but uses more memory)
                perform_standard_merge();
            }
            new_entries_.clear();
        }

        [[nodiscard]] bool can_use_inplace_merge() const {
            // Check if in-place merge would be beneficial
            return sorted_window_.capacity() >= sorted_window_.size() + new_entries_.size();
        }

        void perform_inplace_merge() {
            // Append new entries to sorted window
            const std::size_t old_size = sorted_window_.size();
            sorted_window_.insert(sorted_window_.end(),
                                  std::make_move_iterator(new_entries_.begin()),
                                  std::make_move_iterator(new_entries_.end()));

            // In-place merge
            std::inplace_merge(sorted_window_.begin(),
                               sorted_window_.begin() + static_cast<long>(old_size),
                               sorted_window_.end(),
                               config_.comparator);
        }

        void perform_standard_merge() {
            merge_buffer_.clear();
            merge_buffer_.reserve(sorted_window_.size() + new_entries_.size());

            std::merge(sorted_window_.begin(),
                       sorted_window_.end(),
                       new_entries_.begin(),
                       new_entries_.end(),
                       std::back_inserter(merge_buffer_),
                       config_.comparator);

            sorted_window_ = std::move(merge_buffer_);
            merge_buffer_.clear();
        }

        [[nodiscard]] size_t calculate_output_count() const {
            if (sorted_window_.size() < config_.window_size) {
                return 0;  // Window not exceeded, keep everything
            }

            // Calculate how many entries to flush to maintain window size
            size_t excess = sorted_window_.size() - config_.window_size;

            // Flush at least batch_size entries when window is exceeded
            return std::max(excess, config_.batch_size);
        }

        void output_entries(std::size_t count) {
            if (count == 0 || sorted_window_.empty())
                return;

            // Create output vector
            std::vector<T> output;
            output.reserve(count);

            auto end_it = sorted_window_.begin() + static_cast<long>(std::min(count, sorted_window_.size()));
            output.insert(
                output.end(), std::make_move_iterator(sorted_window_.begin()), std::make_move_iterator(end_it));

            // Remove output entries from window
            sorted_window_.erase(sorted_window_.begin(), end_it);

            // Call consumer
            const std::size_t output_size = output.size();
            consumer_(std::move(output));
            total_processed_ += output_size;
        }

        void maintain_window_size() {
            if (sorted_window_.size() > config_.window_size) {
                const std::size_t excess = sorted_window_.size() - config_.window_size;
                sorted_window_.erase(sorted_window_.begin(), sorted_window_.begin() + static_cast<long>(excess));
            }
        }

        // Optimized insertion sort for small sequences
        template <typename Iterator, typename Compare>
        void insertion_sort(Iterator first, Iterator last, Compare comp) {
            for (auto it = first + 1; it != last; ++it) {
                auto key = std::move(*it);
                auto pos = std::upper_bound(first, it, key, comp);
                std::move_backward(pos, it, it + 1);
                *pos = std::move(key);
            }
        }
    };
}  // namespace demiplane::algorithms
