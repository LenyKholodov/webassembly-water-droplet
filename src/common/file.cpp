#include <common/file.h>
#include <common/exception.h>

#include <cstdio>

namespace engine {
namespace common {

std::string load_file_as_string(const char* file_name)
{
  engine_check_null(file_name);

  FILE* file = fopen(file_name, "rt");

  if (!file)
    throw Exception::format("File '%s' not found", file_name);

  try
  {
    fseek (file, 0, SEEK_END);

    long length = ftell(file);

    fseek (file, 0, SEEK_SET);

    std::string result(size_t(length), '\0');

    size_t read_size = fread(&result[0], 1, length, file);

    result.resize(read_size);

    fclose(file);

    return result;
  }
  catch (...)
  {
    fclose(file);
    throw;
  }
}

}}
