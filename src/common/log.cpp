#include <common/log.h>
#include <common/string.h>
#include <ctime>
#include <cstdio>

#include <sys/time.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

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

#ifndef EMSCRIPTEN
  char time_buffer[128];  
  timeval current_time;

  gettimeofday(&current_time, nullptr);    
  strftime(time_buffer, sizeof time_buffer, "%F %T", localtime(&current_time.tv_sec));

  unsigned int milliseconds = ((unsigned int)(current_time.tv_usec % 1000000)) / 1000;  

  const char* log_level_string = get_log_level_string(log_level);
#endif

  char location_buffer[128] = {0};

  if (context)
  {
    engine::common::xsnprintf(location_buffer, sizeof location_buffer, "%s(%d)", context->function, context->line);
  }

#ifdef EMSCRIPTEN
  char output_buffer[512] = {0};

  engine::common::xsnprintf(output_buffer, sizeof output_buffer, "%s: ", location_buffer);

  int len = strlen(output_buffer);
  engine::common::xvsnprintf(output_buffer + len, sizeof output_buffer - len, format, args);

  EM_ASM(
    var s = Module.UTF8ToString($0, $1);
    switch ($2) {
      case 0: //fatal
      case 1: //error
        console.error(s);
        break;
      case 2: //warning
        console.warn(s);
        break;
      case 3: //info
        console.info(s);
        break;
      case 4: //debug
        console.debug(s);
        break;
      case 5: //trace
        console.trace(s);
        break;
      default:
        console.log(s);
        break;
    }
  , output_buffer, strlen(output_buffer), log_level);
#else
  fprintf(stderr, "%s.%03u [%5s] %30s: ", time_buffer, milliseconds, log_level_string, location_buffer);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  fflush(stderr);
#endif
}

}}}

