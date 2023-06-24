#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <cstdarg>

namespace engine {
namespace common {

/// Portable wrappers on top of snprintf
int xsnprintf(char* buffer, size_t count, const char* format, ...);
int xvsnprintf(char* buffer, size_t count, const char* format, va_list list);

/// Format message
std::string format(const char* format, ...);
std::string vformat(const char* format, va_list args);

/// Tokenize string
std::vector<std::string> split(const char* str, const char* delimiters=" ", const char* spaces=" \t", const char* brackets="");

/// Filename utilities
std::string basename(const char* src);
std::string suffix (const char* src);
std::string dir(const char* src);
std::string notdir(const char* src);

/// Wildcard search
bool wcmatch (const char* str, const char* pattern);

/// String reference
struct StringRef
{
  public:
    StringRef(const char* string);
    StringRef(const std::string& string);

    /// Access to the data
    const char* data() const { return start; }

    /// Number of characters
    size_t size() const { return end - start; }

    /// Convert to string
    std::string to_string() const;

    /// Compare string
    bool operator == (const StringRef&) const;
    bool operator != (const StringRef&) const;

  private:
    const char *start, *end;
};

/// Hash for string
struct StringHash
{
  public:
    StringHash(const StringRef&);

    size_t get() const { return hash; }

    bool operator == (const StringHash&) const;
    bool operator != (const StringHash&) const;

    struct Hasher
    {
      size_t operator()(const StringHash& hash) const { return hash.hash; }
    };

  private:
    static size_t compute_hash(const char*, size_t);

  private:
    size_t hash;
};

#include <common/detail/string.inl>

}}
