#pragma once
#include <mutex>
#include <shared_mutex>

namespace demiplane::multithread {
    template<typename T>
class SyncResource {


    public:
        // Read-only proxy (shared lock)
        class ReadProxy {
        public:
            ReadProxy(std::shared_mutex& mutex, const T& resource)
                : lock_(mutex), resource_(resource) {}

            const T* operator->() const { return &resource_; }
            const T& operator*() const { return &resource_; }
        private:
            std::shared_lock<std::shared_mutex> lock_;
            const T& resource_;
        };

        // Write proxy (exclusive lock)
        class WriteProxy {
        public:
            WriteProxy(std::shared_mutex& mutex, T& resource)
                : lock_(mutex), resource_(resource) {}

            T* operator->() { return &resource_; }
            const T* operator->() const { return &resource_; }

            T& operator*() { return resource_; }
            const T& operator*() const { return &resource_; }
        private:
            std::unique_lock<std::shared_mutex> lock_;
            T& resource_;
        };

        // Constructors
        template<typename... Args>
        explicit SyncResource(Args&&... args) : resource_(std::forward<Args>(args)...) {}

        // Write access (exclusive lock)
        WriteProxy operator->() {
            return WriteProxy(mutex_, resource_);
        }

        WriteProxy write() {
            return WriteProxy(mutex_, resource_);
        }

        // Read access (shared lock)
        ReadProxy read() const {
            return ReadProxy(mutex_, resource_);
        }

        ReadProxy operator->() const {
            return ReadProxy(mutex_, resource_);
        }

        // Functional access for complex operations
        template<typename Func>
        auto with_lock(Func&& func) {
            std::unique_lock lock(mutex_);
            return func(resource_);
        }

        template<typename Func>
        auto with_read_lock(Func&& func) const {
            std::shared_lock lock(mutex_);
            return func(resource_);
        }
    private:
        mutable std::shared_mutex mutex_; // Allow reader-writer locks
        T resource_;
    };


}