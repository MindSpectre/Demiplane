#pragma once

#include "../log_level.hpp"

namespace demiplane::scroll {
    /**
     * @class Entry
     * @brief Represents a base-level log entry with essential logging details.
     *
     * The Entry class provides an abstraction for a log entry, encapsulating
     * information such as the log level, message content, and the location
     * in the source code where the log was generated.
     * Derived classes are expected to implement the to_string() function
     * for proper formatting of log entries.
     */
    class Entry {
        /**
         * @brief Virtual destructor for the Entry class.
         *
         * Ensures proper cleanup of derived class objects when deleted through
         * a pointer to the base class. Defined as default for simplicity and
         * to allow compiler-generated destructor implementation.
         */
    public:
        virtual ~Entry() = default;
        /**
         * @brief Constructs an Entry object with specified logging details.
         *
         * This constructor initializes an Entry instance encapsulating the log level,
         * the actual log message, and the specific source code location where the log occurred.
         *
         * @param level The severity level of the log (e.g., INFO, DEBUG, ERROR).
         * @param message The log message content as a string.
         * @param file The name of the source file where the log was generated.
         * @param line The line number in the source file where the log was created.
         * @param function The name of the function where the log was generated.
         */
        Entry(const LogLevel level, const std::string_view& message, const std::string_view& file, const uint32_t line,
            const std::string_view& function)
            : level_(level), message_(message), file_(file), line_(line), function_(function) {}
        /**
         * @brief Converts the object to its string representation.
         *
         * This pure virtual function is intended to be overridden by derived classes
         * to provide a string representation of the object. Implementations should
         * define the format and content of the resulting string based on the specific
         * requirements of the derived class.
         *
         * @return A string representing the object.
         */
        [[nodiscard]] virtual std::string to_string() const = 0;
        /**
         * @brief Retrieves the log level of the current entry.
         *
         * Provides access to the log level associated with this log entry.
         * The log level indicates the severity or importance of the log message.
         *
         * @return The log level of the entry.
         */
        [[nodiscard]] LogLevel level() const {
            return level_;
        }

        /**
         * @variable level_
         * @brief Represents the logging level for a log entry.
         *
         * This variable determines the severity or importance of a log entry,
         * controlling its categorization and potential filtering. Possible
         * values are defined in the LogLevel enumeration, such as Debug, Info,
         * Warning, and Error, among others.
         */
    protected:
        LogLevel level_{LogLevel::Debug};
        /**
         * @variable message_
         * @brief Stores the content of the log message.
         *
         * Represents the main textual content of a log entry. It is used for encoding
         * the information or description relevant to the log event. This variable is
         * typically integrated into the formatted output during the logging process.
         */
        std::string_view message_;
        /**
         * @variable file_
         * @brief Represents the name of the file associated with a log entry.
         *
         * This variable stores a constant view of the file name where the log entry
         * was generated.
         * It is used to provide context about the source of the
         * log, typically used in formatted log output.
         */
        std::string_view file_;
        /**
         * @variable line_
         * @brief Represents the line number in the source code associated with a log entry.
         *
         * This variable stores the specific line number in the file where the log entry
         * is generated, aiding in pinpointing the exact location in the source code
         * for debugging or traceability purposes.
         */
        uint32_t line_;
        /**
         * @variable function_
         * @brief Represents the name of the function associated with a log entry.
         *
         * This variable holds a view of the function name where the log was generated,
         * allowing inclusion of contextual information about the source code in the log entry.
         * Its usage is optional and controlled by the logging configuration.
         */
        std::string_view function_;
    };
    template <typename AnyEntry>
    concept IsEntry = std::is_base_of_v<Entry, AnyEntry>;

} // namespace demiplane::scroll
