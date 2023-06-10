#include <common/exception.h>
#include <common/string.h>
#include <string>
#include <vector>

#include <cxxabi.h>

#ifdef __APPLE__
  #include <execinfo.h>
#endif

using namespace engine::common;

/// Internal exception details
struct Exception::Impl
{
  std::string message;

  Impl(std::string&& message)
    : message(message)
  {
  }

#ifdef __APPLE__
  static std::string format_symbol_info(const char* string)
  {
    std::vector<std::string> tokens = engine::common::split(string);

    static const size_t EXPECTED_TOKENS_COUNT = 6;

    if (tokens.size() != EXPECTED_TOKENS_COUNT)
      return string;

    int status = 0;
    const char* demangled_name = abi::__cxa_demangle(tokens[3].c_str(), 0, 0, &status);

    if (!demangled_name)
      demangled_name = tokens[3].c_str();

    return demangled_name;
  }
#endif  

  static std::string get_stack_trace(int offset = 0)
  {
    std::string message;

#ifdef __APPLE__
    static const int BACKTRACE_MAX_SIZE = 128;

    std::vector<void*> stack;

    stack.resize(BACKTRACE_MAX_SIZE);

    const int size = backtrace(&stack[0], (int)stack.size());

    std::unique_ptr<char *, decltype(&::free)> stack_string_list(backtrace_symbols(&stack[0], size), &::free);

    int stack_frame_index = 0;

    for (int i=offset; i<size; i++)
    {
      const char* stack_string = stack_string_list.get()[i];
      std::string stack_string_formatted = format_symbol_info(stack_string);

      static const char* EXCEPTION_CLASS_PREFIX = "engine::common::Exception::";
      static const size_t EXCEPTION_CLASS_PREFIX_LEN = strlen(EXCEPTION_CLASS_PREFIX);

      if (!strncmp(EXCEPTION_CLASS_PREFIX, stack_string_formatted.c_str(), EXCEPTION_CLASS_PREFIX_LEN))
        continue;

      message += engine::common::format("\n    at %3u: %s", stack_frame_index, stack_string_formatted.c_str());
      stack_frame_index++;
    }
#endif

    return message;
  }
};

Exception::Exception(const char* message)
{
  if (!message)
    message = "";

  std::string full_message = message + Impl::get_stack_trace();

  impl = std::make_shared<Impl>(std::move(full_message));
}

Exception Exception::format(const char* format, ...)
{
  va_list list;  

  va_start(list, format);

  return vformat(format, list);
}

Exception Exception::vformat(const char* format, va_list args)
{
    return Exception(engine::common::vformat(format, args).c_str());
}

const char* Exception::what() const noexcept
{
  return impl->message.c_str();
}

namespace engine {
namespace common {

Exception make_null_argument_exception(const char* param_name)
{
  if (!param_name)
    param_name = "<unknown>";
  
  return Exception::format("Null argument '%s'", param_name);
}

Exception make_range_exception(const char* param_name, double value, double min, double max)
{
  if (!param_name)
    param_name = "<unknown>";

  return Exception::format("Argument '%s'=%g is out of range [%g; %g)", param_name, value, min, max); 
}

Exception make_range_exception(const char* param_name, double value, double max)
{
  return make_range_exception(param_name, value, 0.0, max);
}

Exception make_not_implemented_exception(const char* method_name)
{
  if (method_name && *method_name)
    return Exception::format("Method is not implemented '%s'", method_name);

  return Exception("Method is not implemented");
}

[[ noreturn ]] void unimplemented(const char* method_name)
{
  throw make_not_implemented_exception(method_name);
}

}}
