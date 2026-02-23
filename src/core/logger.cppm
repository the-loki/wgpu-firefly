module;
#include <memory>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

export module firefly.core.logger;

import firefly.core.types;

export namespace firefly {

enum class LogLevel : u8 {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Off
};

class Logger {
public:
    static void init(const String& name = "firefly",
                     LogLevel level = LogLevel::Debug);
    static void shutdown();

    static void set_level(LogLevel level);

    template<typename... Args>
    static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->trace(fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->debug(fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->info(fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->warn(fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->error(fmt, std::forward<Args>(args)...);
    }
    template<typename... Args>
    static void fatal(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        s_logger->critical(fmt, std::forward<Args>(args)...);
    }

private:
    static inline std::shared_ptr<spdlog::logger> s_logger;
};

} // namespace firefly
