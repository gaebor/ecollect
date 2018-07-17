#include "DataTypes.h"

#include "Utils.h"

#include <cstring>
#include <cstdlib>
#include <string>

size_t DataView<true>::size = 0;
const char* DataView<false>::separators = "";

//bool DataView<false>::ReadFrom(char*& buffer, char* end) noexcept
//{
//    while (buffer < end && strchr(separators, *buffer) != NULL)
//        *buffer++ = '\0';
//    ptr = buffer;
//    while (buffer < end && strchr(separators, *buffer) == NULL)
//        ++buffer;
//    if (buffer == end)
//    {   // no separator at the end, setting back to last known place
//        size = 0;
//        buffer = (char*)ptr;
//        ptr = nullptr;
//        return false;
//    }
//    else
//    {
//        *buffer++ = '\0';
//        size = buffer - ptr;
//        return true;
//    }
//}
// 
//template<>
//bool RecordView<false>::DumpTo(FILE* f)const noexcept
//{
//    // fprintf(f, "%s%c", ptr, separator);
//    return fwrite(ptr, size - 1, 1, f) == 1 && fputc(separator, f) != EOF &&
//        fprintf(f,
//#if defined(_MSC_VER) && _MSC_VER < 1900 
//        "%Iu\n"
//#else
//        "%zu\n"
//#endif
//        , count) > 0;
//}

template<>
bool Packet<DataView<true>>::ReadFrom(FILE* f)
{
    resize(view.size);
    if (fread(data(), view.size, 1, f))
    {
        view.ptr = data();
        return true;
    }
    else
        return false;
}

template<>
bool Packet<RecordView<true>>::ReadFrom(FILE* f)
{
    resize(view.size);
    if (fread(data(), view.size, 1, f) == 1 &&
        fread(&view.count, sizeof(view.count), 1, f) == 1)
    {
        view.ptr = data();
        return true;
    }
    else
        return false;
}

template<>
bool Packet<DataView<false>>::ReadFrom(FILE* f)
{
    clear();
    int c;
    while ((c = fgetc(f)) != EOF && (char)c != view.separators[0])
    {
        emplace_back((char)c);
    }
    emplace_back('\0');
    view.size = size();
    view.ptr = data();
    return c != EOF || size() > 1;
}

template<>
bool Packet<RecordView<false>>::ReadFrom(FILE* f)
{
    clear();
    int c;
    size_t sep = std::string::npos;
    while ((c = fgetc(f)) != EOF && (char)c != '\n')
    {
        emplace_back((char)c);
        if ((char)c == view.separator)
            sep = size();
    }
    emplace_back('\0');

    if (sep != std::string::npos)
    {
        operator[](sep - 1) = '\0';
        view.count = (size_t)atoll(data() + sep);
        resize(sep);
        view.size = size();
        view.ptr = data();
        return true;
    }
    else
        return false;
}

void SetSeparator(const char* sep)
{
    DataView<false>::separators = sep;
    RecordView<false>::separator = (sep[0] == '\n' ? ' ' : sep[0]);
}

void SetBinary(size_t binary_size)
{
    RecordView<true>::size = binary_size;
}
