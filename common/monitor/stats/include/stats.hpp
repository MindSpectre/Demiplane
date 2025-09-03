#pragma once
#include <chrono>
#include <string>
#include <utility>

#include <json/json.h>
namespace demiplane::monitor {
    // TODO: template supposed to be json
    class Stats {
    public:
        explicit Stats(std::string name) {
            instance_   = std::move(name);
            time_point_ = std::chrono::system_clock::now();
        }
        virtual ~Stats()                                  = default;
        [[nodiscard]] virtual std::string convert() const = 0;
        [[nodiscard]] const std::string& get_instance() const {
            return instance_;
        }
        void set_instance(std::string instance) {
            instance_ = std::move(instance);
        }
        [[nodiscard]] const std::chrono::time_point<std::chrono::system_clock>& get_time_point() const {
            return time_point_;
        }
        void set_time_point(const std::chrono::time_point<std::chrono::system_clock>& time_point) {
            time_point_ = time_point;
        }

    protected:
        std::string instance_;
        std::chrono::time_point<std::chrono::system_clock> time_point_;
    };

    class JsonStats final : public Stats {
    public:
        ~JsonStats() override;
        explicit JsonStats(std::string name)
            : Stats(std::move(name)) {
        }
        [[nodiscard]] std::string convert() const override {
            return data.toStyledString();
        }
        void add(const std::string& key, const std::string& value) noexcept {
            data[key] = value;
        }
        void remove(const std::string& key) {
            if (data.isMember(key)) {
                data.removeMember(key);
            } else {
                throw std::invalid_argument("Key not found");
            }
        }

    private:
        Json::Value data;
    };
}  // namespace demiplane::monitor
