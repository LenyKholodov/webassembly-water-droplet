#include <common/string.h>

namespace engine {
namespace common {

int xsnprintf(char* buffer, size_t count, const char* format, ...)
{
  va_list list;

  va_start(list, format);

  return xvsnprintf(buffer, count, format, list);
}

#ifdef _MSC_VER

int xvsnprintf(char* buffer, size_t count, const char* format, va_list list)
{
  if (!buffer || !count)
    return count ? -1 : _vscprintf(format, list);

  int ret = ::_vsnprintf(buffer, count-1, format, list);

  buffer[count-1] = '\0';

  return ret < 0 || (size_t)ret >= count ? -1 : ret;
}

#else

int xvsnprintf(char* buffer, size_t count, const char* format, va_list list)
{
  if (!buffer || !count)
    return count ? -1 : ::vsnprintf(0, 0, format, list);

  int ret = ::vsnprintf(buffer, count, format, list);

  buffer[count-1] = '\0';

  return ret < 0 || (size_t)ret >= count ? -1 : ret;
}

#endif

std::string vformat(const char* format, va_list list)
{
  if (!format)
    return "";

  va_list list_copy;

  va_copy(list_copy, list);

  int size = xvsnprintf(0, 0, format, list_copy);

  va_end(list_copy);

  if (size == -1)
    return "";

  std::string str;

  str.resize(size);

  xvsnprintf(&str[0], size+1, format, list);

  return str;
}

std::string format(const char* format, ...)
{
  va_list list;  

  va_start(list, format);

  return vformat(format, list);
}

namespace
{

class WordParser
{
  public:
    WordParser(const char* string, const char* delimiters, const char* spaces, const char* brackets) : pos ((unsigned char*)string)
    {
      memset(delimiters_map, 0, sizeof delimiters_map);
      memset(space_map, 0, sizeof space_map);
      memset(open_brackets_map, 0, sizeof open_brackets_map);
      memset(close_brackets_map, 0, sizeof close_brackets_map);

      for (; *delimiters; delimiters++) delimiters_map[(unsigned char)*delimiters] = true;
      for (; *spaces; spaces++)         space_map[(unsigned char)*spaces] = true;

      for (; brackets[0] && brackets[1]; brackets += 2)
      {
        open_brackets_map[(unsigned char)brackets [0]] = true;
        close_brackets_map[(unsigned char)brackets [1]] = true;
      }

      delimiters_map[(unsigned char)'\0'] = true;
      close_brackets_map[(unsigned char)'\0'] = true;
    }

    typedef std::pair<const char*, const char*> Word;

    Word next_word ()
    {
      if (!*pos)
        return Word((const char*)pos, (const char*)pos);

      for (; space_map[*pos]; pos++); //cut leading spaces

      const unsigned char *first, *last;
      bool word_in_brackets = false;

      if (open_brackets_map[*pos]) //found open bracket
      {
        first = ++pos;

        for (; !close_brackets_map[*pos]; pos++);

        last = pos;

        if (pos[0] && delimiters_map[pos[1]])
          ++pos;

        word_in_brackets = true;
      }
      else
      {
        first = pos;

        for (; !delimiters_map[*pos]; pos++);

        last = pos;
      }

      if (*pos)
        pos++;

      if (last != first && !word_in_brackets)
      {
        for (--last; space_map[*last]; last--); //cut trailing spaces

        ++last;
      }

      return Word((const char*)first, (const char*)last);
    }

    bool is_eos () const { return *pos == '\0'; }

  private:
    typedef bool BoolMap [256];

  private:
    BoolMap delimiters_map, space_map, open_brackets_map, close_brackets_map;
    unsigned char* pos;
};

}

std::vector<std::string> split (const char* str, const char* delimiters, const char* spaces, const char* brackets)
{
  std::vector<std::string> res;

  if (!str || !*str || !delimiters || !spaces || !brackets)
    return res;

  res.reserve(8);

  WordParser parser(str, delimiters, spaces, brackets);

  do
  {
    WordParser::Word word = parser.next_word();

    if (word.first != word.second || !parser.is_eos())
      res.push_back(std::string(word.first, word.second - word.first));

  } while (!parser.is_eos());

  return res;
}

std::string basename(const char* src)
{
  size_t len = strlen(src);

  for (const char* s=src+len; len--;)
    if (*--s == '.')
      return std::string(src, s-src);

  return std::string(src);
}

std::string suffix (const char* src)
{
  size_t len = strlen(src);

  for (const char* s=src+len; s!=src;)
    if (*--s == '.')
      return std::string(s, len-(s-src));

  return std::string();
}

std::string dir(const char* src)
{
  size_t len = strlen(src);

  for (const char* s=src+len; len--;)
    if (*--s == '/')
      return std::string(src, s-src+1);

  return std::string("./");
}

std::string notdir(const char* src)
{
  size_t len = strlen(src);

  for (const char* s=src+len; s!=src;)
    if (*--s == '/')
      return std::string(s+1, len-(s-src)-1);

  return std::string(src);
}

bool wcmatch(const char* s, const char* pattern)
{
  if (!pattern && !s)
    return true;

  if (!pattern)
    return false;

  if (!s)
    return false;

  while (*s)
    switch (*pattern)
    {
      case '\0':
        return false;
      case '*':
        do pattern++; while (*pattern == '*' || *pattern == '?');
        for (s=strchr(s,*pattern);s && !wcmatch(s,pattern);s=strchr(s+1,*pattern));

        return s != NULL;
      case '?':
        return wcmatch(s,pattern+1) || wcmatch(s+1,pattern+1);
      default:
        if (*pattern++ != *s++)
          return false;

        break;
    }
   
  while (*pattern)
    switch (*pattern++)
    {
      case '*':
      case '?': break;
      default:  return false;     
    }

  return true;
}

}}
