#pragma once

#include <atomic>
#include <demiplane/multithread>
#include <format>
#include <future>
#include <memory>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include "logger_config.hpp"
#include "sink_interface.hpp"
namespace demiplane::scroll {
    /**
     * @brief High-performance asynchronous logger using Disruptor pattern
     *
     * Features:
     * - Lock-free multi-producer logging via Disruptor ring buffer
     * - Single consumer thread batches and dispatches to sinks
     * - Non-templated (stores heterogeneous sinks via base class)
     * - Support for both format strings and stream-based logging
     * - Graceful shutdown ensures all events are processed
     *
     * Architecture:
     *   Producer threads → RingBuffer<LogEvent, 8192> → Consumer thread → Sinks
     *
     * Performance:
     * - ~10M events/sec throughput
     * - Sub-microsecond latency (P99 < 1μs)
     * - Zero heap allocations on hot path (pre-allocated ring buffer)
     *
     * Usage:
     *   Logger logger;
     *   logger.add_sink(std::make_unique<ConsoleSink<DetailedEntry>>(...));
     *   logger.add_sink(std::make_unique<FileSink<LightEntry>>(...));
     *
     *   // Format string style
     *   logger.log(LogLevel::Info, "User {} logged in", username);
     *
     *   // Stream style
     *   logger.stream(LogLevel::Info) << "User " << username << " logged in";
     */
    /**
     * @brief Per-sink slot: pairs a sink with its asio::strand for serial dispatch
     */
    struct SinkSlot {
        std::shared_ptr<Sink> sink;
        boost::asio::strand<boost::asio::any_io_executor> strand;
    };

    class Logger {
    public:
        /**
         * @brief Construct with external executor (recommended)
         *
         * The executor's thread pool runs sink processing. Multiple loggers
         * can share the same executor (e.g., with HTTP/DB components).
         */
        constexpr explicit Logger(boost::asio::any_io_executor executor,
                                  const LoggerConfig& cfg = LoggerConfig::Builder{}.finalize())
            : disruptor_{cfg.ring_buffer_size(), create_wait_strategy(cfg.wait_strategy())},
              executor_{std::move(executor)} {
            running_.store(true, std::memory_order_release);
            consumer_thread_ = std::jthread([this] { consumer_loop(); });
        }

        /**
         * @brief Construct with internal thread pool (backward-compatible)
         *
         * Creates an internal asio::thread_pool sized by LoggerConfig::pool_size.
         */
        constexpr explicit Logger(const LoggerConfig& cfg = LoggerConfig::Builder{}.finalize())
            : disruptor_{cfg.ring_buffer_size(), create_wait_strategy(cfg.wait_strategy())},
              owned_pool_{std::in_place, cfg.pool_size()},
              executor_{owned_pool_->get_executor()} {
            running_.store(true, std::memory_order_release);
            consumer_thread_ = std::jthread([this] { consumer_loop(); });
        }

        ~Logger() {
            shutdown();
        }

        /**
         * @brief Add a sink to receive log events
         * @param sink Shared pointer to sink (ConsoleSink, FileSink, custom)
         *
         * Each sink is paired with a strand on the executor for serial dispatch.
         * Can be called before logging starts. NOT thread-safe during logging.
         */
        void add_sink(std::shared_ptr<Sink> sink) {
            sink_slots_.push_back(SinkSlot{std::move(sink), boost::asio::make_strand(executor_)});
        }

        /**
         * @brief Log with format string (C++20 std::format) + prefix
         * @param lvl Log level
         * @param prefix Logger / class prefix; empty if none
         * @param loc Source location
         * @param fmt Format string
         * @param args Arguments to format
         */
        template <typename... Args>
        constexpr void log(const LogLevel lvl,
                           const std::string_view prefix,
                           const std::source_location& loc,
                           std::format_string<Args...> fmt,
                           Args&&... args) {
            // COROUTINE SAFETY: No suspension points allowed between tl_msg_buf usage and swap.
            thread_local std::string tl_msg_buf;
            tl_msg_buf.clear();
            std::format_to(std::back_inserter(tl_msg_buf), fmt, std::forward<Args>(args)...);
            const auto meta = EventMeta{lvl, loc};

            const std::int64_t seq = disruptor_.sequencer().next();
            auto& event            = disruptor_.ring_buffer()[seq];

            event.message.swap(tl_msg_buf);
            event.prefix.assign(prefix);
            apply_meta(event, meta);

            disruptor_.sequencer().publish(seq);
            // TODO(scroll/logger): formatted-message cost is paid even when all
            // sinks filter the event out. Consider moving format to sinks
            // (requires type-erased args) or querying sink filters before format.
        }

        /**
         * @brief Log with a simple message + prefix
         */
        constexpr void log(const LogLevel lvl,
                           const std::string_view prefix,
                           const std::string_view msg,
                           const std::source_location& loc = std::source_location::current()) {
            thread_local std::string tl_msg_buf;
            tl_msg_buf.clear();
            tl_msg_buf.append(msg);
            const auto meta = EventMeta{lvl, loc};

            const std::int64_t seq = disruptor_.sequencer().next();
            auto& event            = disruptor_.ring_buffer()[seq];

            event.message.swap(tl_msg_buf);
            event.prefix.assign(prefix);
            apply_meta(event, meta);

            disruptor_.sequencer().publish(seq);
        }

        /**
         * @brief Shorthand: log with simple message, no prefix.
         *
         * Kept permanently for callers that don't care about prefix (tests,
         * utility code). Forwards to the prefix-aware overload with empty prefix.
         */
        constexpr void log(const LogLevel lvl,
                           const std::string_view msg,
                           const std::source_location& loc = std::source_location::current()) {
            log(lvl, std::string_view{}, msg, loc);
        }

        /**
         * @brief Stream-based logging proxy carrying level + prefix + source location
         */
        class StreamProxy {
        public:
            template <typename SourceLocationTp = std::source_location>
                requires std::is_same_v<std::remove_cvref_t<SourceLocationTp>, std::source_location>
            constexpr StreamProxy(Logger* logger,
                                  const LogLevel lvl,
                                  const std::string_view prefix,
                                  SourceLocationTp&& loc) noexcept
                : logger_{logger},
                  level_{lvl},
                  loc_{std::forward<SourceLocationTp>(loc)},
                  prefix_{} {
                prefix_.assign(prefix);  // runtime: truncates silently at PrefixNameStorage capacity
            }

            template <typename T>
            StreamProxy& operator<<(const T& value) {
                stream_ << value;
                return *this;
            }

            constexpr ~StreamProxy() noexcept {
                thread_local std::string tl_msg_buf;
                tl_msg_buf.clear();
                tl_msg_buf.append(stream_.view());
                const auto meta = EventMeta{level_, loc_};

                const std::int64_t seq = logger_->disruptor_.sequencer().next();
                auto& event            = logger_->disruptor_.ring_buffer()[seq];

                event.message.swap(tl_msg_buf);
                event.prefix.assign(prefix_.view());
                apply_meta(event, meta);

                logger_->disruptor_.sequencer().publish(seq);
            }

        private:
            Logger* logger_;
            LogLevel level_;
            std::source_location loc_;
            PrefixNameStorage prefix_;
            std::ostringstream stream_;
        };

        template <typename SourceLocationTp = std::source_location>
            requires std::is_same_v<std::remove_cvref_t<SourceLocationTp>, std::source_location>
        constexpr StreamProxy
        stream(const LogLevel lvl, std::string_view prefix, SourceLocationTp loc = std::source_location::current()) {
            return StreamProxy{this, lvl, prefix, std::forward<SourceLocationTp>(loc)};
        }

        /**
         * @brief Graceful shutdown - waits for all events to be processed
         *
         * Called automatically by destructor.
         */
        void shutdown();

        void flush() const {
            for (const auto& [sink, strand] : sink_slots_) {
                boost::asio::post(strand, [sink] { sink->flush(); });
            }
        }

    private:
        multithread::DynamicDisruptor<LogEvent> disruptor_;
        std::optional<boost::asio::thread_pool> owned_pool_;
        boost::asio::any_io_executor executor_;
        std::vector<SinkSlot> sink_slots_;
        std::jthread consumer_thread_;
        std::atomic<bool> running_{false};

        /**
         * @brief Pre-captured metadata (built outside the CAS critical path)
         */
        struct EventMeta {
            LogLevel level;
            detail::MetaSource location;
            detail::MetaTimePoint time_point;
            detail::MetaThread tid;
            detail::MetaProcess pid;

            constexpr EventMeta(const LogLevel lvl, const std::source_location& loc) noexcept
                : level{lvl},
                  location{loc} {
            }
        };

        /**
         * @brief Apply pre-captured metadata to ring buffer slot (minimal critical path)
         */
        constexpr static void apply_meta(LogEvent& event, const EventMeta& meta) noexcept {
            event.level           = meta.level;
            event.location        = meta.location;
            event.time_point      = meta.time_point;
            event.tid             = meta.tid;
            event.pid             = meta.pid;
            event.shutdown_signal = false;
        }

        /**
         * @brief Consumer thread loop - processes events and dispatches to sinks
         */
        void consumer_loop();

        /**
         * @brief Create wait strategy based on config
         */
        constexpr static std::unique_ptr<multithread::WaitStrategy>
        create_wait_strategy(const LoggerConfig::WaitStrategy strategy) {
            using namespace multithread;

            switch (strategy) {
                case LoggerConfig::WaitStrategy::BusySpin:
                    return std::make_unique<BusySpinWaitStrategy>();
                case LoggerConfig::WaitStrategy::Yielding:
                    return std::make_unique<YieldingWaitStrategy>();
                case LoggerConfig::WaitStrategy::Blocking:
                    return std::make_unique<BlockingWaitStrategy>();
                default:
                    return std::make_unique<YieldingWaitStrategy>();
            }
        }
    };
}  // namespace demiplane::scroll
