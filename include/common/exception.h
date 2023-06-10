#pragma once

#include <exception>
#include <memory>
#include <cstdarg>

namespace engine {
namespace common {

/// Basic exception with stack tracing
class Exception: public std::exception
{
  public:
    /// Constructor
    Exception(const char* message);

    /// Create exception with formatted message
    static Exception format(const char* format, ...);
    static Exception vformat(const char* format, va_list args);

    /// Exception reason override
    const char* what() const noexcept;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Create null argument exception
Exception make_null_argument_exception(const char* param_name);

/// Create range exception
Exception make_range_exception(const char* param_name, double value, double min, double max);

/// Create range exception
Exception make_range_exception(const char* param_name, double value, double max);

/// Create exception to notify about unimplemented method
Exception make_not_implemented_exception(const char* method_name="");

/// Create exception to notify about unimplemented method and raise it
[[ noreturn ]] void unimplemented(const char* method_name="");

/// Assertion check; throws exception in case of error
#define engine_check(X) if (!(X)) throw engine::common::Exception::format("Assertion failed: %s", #X)

/// Assertion check; check range
#define engine_check_range(X, MAX) if ((X) >= (MAX)) throw engine::common::make_range_exception(#X, (X), (MAX))

/// Assertion check; check null
#define engine_check_null(X) if (!(X)) throw engine::common::make_null_argument_exception(#X)

}}
