#pragma once

#include <cstdarg>

namespace engine {
namespace common {
namespace log {

/// Logging level
enum LogLevel
{
  Fatal,
  Error,
  Warning,
  Info,
  Debug,
  Trace,
};

/// Logging context
struct LogContext
{
  const char* function;
  const char* file;
  int line;
};

/// Basic logging functions
void printf(LogLevel log_level, const LogContext* context, const char* format, ...);
void vprintf(LogLevel log_level, const LogContext* context, const char* format, va_list args);

}}}

/// Logging macroses

#ifdef WIN32
    #define LOG_FUNCTION_NAME   __FUNCTION__  
#else
    #define LOG_FUNCTION_NAME   __func__ 
#endif

#define engine_log_printf(log_level, format, ...) { \
 static const engine::common::log::LogContext context = {LOG_FUNCTION_NAME, __FILE__, __LINE__}; \
  engine::common::log::printf(log_level, &context, format, ##__VA_ARGS__); \
}

#define engine_log_fatal(format, ...) engine_log_printf(engine::common::log::Fatal, format, ##__VA_ARGS__)
#define engine_log_error(format, ...) engine_log_printf(engine::common::log::Error, format, ##__VA_ARGS__)
#define engine_log_warning(format, ...) engine_log_printf(engine::common::log::Warning, format, ##__VA_ARGS__)
#define engine_log_info(format, ...) engine_log_printf(engine::common::log::Info, format, ##__VA_ARGS__)
#define engine_log_debug(format, ...) engine_log_printf(engine::common::log::Debug, format, ##__VA_ARGS__)
#define engine_log_trace(format, ...) engine_log_printf(engine::common::log::Trace, format, ##__VA_ARGS__)
