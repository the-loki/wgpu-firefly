module;
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

module firefly.core.logger;

namespace firefly {

static auto to_spdlog_level(LogLevel level) -> spdlog::level::level_enum {
    switch (level) {
        case LogLevel::Trace: return spdlog::level::trace;
        case LogLevel::Debug: return spdlog::level::debug;
        case LogLevel::Info:  return spdlog::level::info;
        case LogLevel::Warn:  return spdlog::level::warn;
        case LogLevel::Error: return spdlog::level::err;
        case LogLevel::Fatal: return spdlog::level::critical;
        case LogLevel::Off:   return spdlog::level::off;
        default:              return spdlog::level::info;
    }
}

void Logger::init(const String& name, LogLevel level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    s_logger = std::make_shared<spdlog::logger>(name, console_sink);
    s_logger->set_level(to_spdlog_level(level));
    s_logger->flush_on(spdlog::level::info);

    spdlog::set_default_logger(s_logger);
}

void Logger::shutdown() {
    spdlog::shutdown();
    s_logger.reset();
}

void Logger::set_level(LogLevel level) {
    if (s_logger) {
        s_logger->set_level(to_spdlog_level(level));
    }
}

} // namespace firefly
