#include <common/log.h>
#include <common/string.h>
#include <ctime>
#include <cstdio>

#include <sys/time.h>

namespace engine {
namespace common {
namespace log {

namespace
{

const char* get_log_level_string(LogLevel level)
{
  switch (level)
  {
    case Fatal:   return "FATAL";
    case Error:   return "ERROR";
    case Warning: return "WARN";
    default:
    case Info:    return "INFO";
    case Debug:   return "DEBUG";
    case Trace:   return "TRACE";
  }
}

}

void printf(LogLevel log_level, const LogContext* context, const char* format, ...)
{
  va_list list;

  va_start(list, format);

  return vprintf(log_level, context, format, list);
}

void vprintf(LogLevel log_level, const LogContext* context, const char* format, va_list args)
{
  if (!format)
    return;

  char time_buffer[128];  
  timeval current_time;

  gettimeofday(&current_time, nullptr);    
  strftime(time_buffer, sizeof time_buffer, "%F %T", localtime(&current_time.tv_sec));

  unsigned int milliseconds = ((unsigned int)(current_time.tv_usec % 1000000)) / 1000;  

  const char* log_level_string = get_log_level_string(log_level);

  char location_buffer[128] = {0};

  if (context)
  {
    engine::common::xsnprintf(location_buffer, sizeof location_buffer, "%s(%d)", context->function, context->line);
  }

  fprintf(stderr, "%s.%03u [%5s] %30s: ", time_buffer, milliseconds, log_level_string, location_buffer);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  fflush(stderr);
}

}}}

