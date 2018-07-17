#include "Utils.h"

#include <cstring>
#include <cstdio>
#include <vector>
#include <type_traits>

#ifdef _MSC_VER
#   include <fcntl.h>
#   include <io.h>
#endif

std::string GetFilename(size_t i, int width, const char* prefix)
{
    std::vector<char> buffer(strlen(prefix) + 20, '\0');
    snprintf(buffer.data(), buffer.size()-1, "%s%0*zu.tmp", prefix, width, i);
    return std::string(buffer.data());
}

bool SetBinaryIO()
{
#ifdef _MSC_VER
    return _setmode(_fileno(stdin), _O_BINARY) != -1 &&
        _setmode(_fileno(stdout), _O_BINARY) != -1;
#else
    return true;
#endif // _MSC_VER
}

bool matches(const char * str, const std::initializer_list<const char*>& patterns)
{
    for (auto pattern : patterns)
    {
        if (strcmp(pattern, str) == 0)
            return true;
    }
    return false;
}

size_t Fnv1a::hash(const char * ptr, size_t size)
{
    size_t result = offset;
    const auto end = ptr + size;
    while (ptr < end)
    {	// fold in another byte
        result ^= (size_t)(*ptr++);
        result *= prime;
    }
    return result;
}

size_t Fnv1a::operator()(const char * ptr, size_t size)const
{
    return hash(ptr, size);
}
