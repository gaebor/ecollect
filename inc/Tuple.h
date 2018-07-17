#pragma once

#include <cstring>
#include <cwchar>
#include <string>

#include "DataTypes.h"
#include "Utils.h"

enum FormatType
{
    SCANF_STRING, //<! s [ ] [^ ]
    SCANF_WSTRING, //<! ls l[ ] l[^ ]
    SCANF_CHAR,
    SCANF_WCHAR, //<! lc
    SCANF_UCHAR,
    SCANF_SHORT,
    SCANF_USHORT,
    SCANF_INT,
    SCANF_UINT,
    SCANF_LONG,
    SCANF_ULONG,
    SCANF_LLONG,
    SCANF_ULLONG,
    SCANF_SIZET, //<! zu
    SCANF_PTR,
    SCANF_FLOAT,
    SCANF_DOUBLE,
    SCANF_LDOUBLE,
    SCANF_UNKNOWN, //! wrong format
};

typedef int(*Comparer)(const char*, const char*); // -1, 0 ,1

FormatType ParseType(const char*& fmt);

template<FormatType type>
int FormatLess(const char* a, const char* b)
{
    switch (type)
    {
    case SCANF_STRING:  return sgn(strcmp(a, b));
    case SCANF_WSTRING: return sgn(wcscmp((wchar_t*)a, (wchar_t*)b));
    case SCANF_CHAR:    return cmp(*(char*)a, *(char*)b);
    case SCANF_WCHAR:   return cmp(*(wchar_t*)a, *(wchar_t*)b);
    case SCANF_UCHAR:   return cmp(*(unsigned char*)a, *(unsigned char*)b);
    case SCANF_SHORT:   return cmp(*(short int*)a, *(short int*)b);
    case SCANF_USHORT:  return cmp(*(unsigned short int*)a ,*(unsigned short int*)b);
    case SCANF_INT:     return cmp(*(int*)a ,*(int*)b);
    case SCANF_UINT:    return cmp(*(unsigned int*)a ,*(unsigned int*)b);
    case SCANF_LONG:    return cmp(*(long int*)a ,*(long int*)b);
    case SCANF_ULONG:   return cmp(*(unsigned long int*)a ,*(unsigned long int*)b);
    case SCANF_LLONG:   return cmp(*(long long int*)a ,*(long long int*)b);
    case SCANF_ULLONG:  return cmp(*(unsigned long long int*)a ,*(unsigned long long int*)b);
    case SCANF_SIZET:   return cmp(*(size_t*)a ,*(size_t*)b);
    case SCANF_PTR:     return cmp(*(void**)a ,*(void**)b);
    case SCANF_FLOAT:   return cmp(*(float*)a ,*(float*)b);
    case SCANF_DOUBLE:  return cmp(*(double*)a ,*(double*)b);
    case SCANF_LDOUBLE: return cmp(*(long double*)a ,*(long double*)b);
    default: return 0;
    };
}

template<FormatType type, bool reverse = false>
inline int FormatComp(const char* a, const char* b)
{
    return (reverse ? -1 : 1) * FormatLess<type>(a, b);
}

template<bool reverse = false>
Comparer MakeComparer(FormatType type)
{
    switch (type)
    {
    case SCANF_STRING:  return FormatComp<SCANF_STRING, reverse>;
    case SCANF_WSTRING: return FormatComp<SCANF_WSTRING, reverse>;
    case SCANF_CHAR:    return FormatComp<SCANF_CHAR, reverse>;
    case SCANF_WCHAR:   return FormatComp<SCANF_WCHAR, reverse>;
    case SCANF_UCHAR:   return FormatComp<SCANF_UCHAR, reverse>;
    case SCANF_SHORT:   return FormatComp<SCANF_SHORT, reverse>;
    case SCANF_USHORT:  return FormatComp<SCANF_USHORT, reverse>;
    case SCANF_INT:     return FormatComp<SCANF_INT, reverse>;
    case SCANF_UINT:    return FormatComp<SCANF_UINT, reverse>;
    case SCANF_LONG:    return FormatComp<SCANF_LONG, reverse>;
    case SCANF_ULONG:   return FormatComp<SCANF_ULONG, reverse>;
    case SCANF_LLONG:   return FormatComp<SCANF_LLONG, reverse>;
    case SCANF_ULLONG:  return FormatComp<SCANF_ULLONG, reverse>;
    case SCANF_SIZET:   return FormatComp<SCANF_SIZET, reverse>;
    case SCANF_PTR:     return FormatComp<SCANF_PTR, reverse>;
    case SCANF_FLOAT:   return FormatComp<SCANF_FLOAT, reverse>;
    case SCANF_DOUBLE:  return FormatComp<SCANF_DOUBLE, reverse>;
    case SCANF_LDOUBLE: return FormatComp<SCANF_LDOUBLE, reverse>;
    default: return FormatComp<SCANF_UNKNOWN, reverse>;
    };
}

Comparer MakeComparer(FormatType type, bool reverse);

struct ParseResult
{
    size_t pos;
    FormatType type;
    bool skipped;
    ParseResult() : pos(0), type(SCANF_UNKNOWN), skipped(false) {}
};

template<bool binary>
struct TupleView;

template<>
struct TupleView<true> : DataView<true>
{
    static std::vector<size_t> offsets; // the fields have a fixed position
    static std::vector<Comparer> comps;

    static bool InitParse(const char* format, const std::vector<int>& keys);

    bool operator<(const TupleView& other)const
    {
        for (size_t k = 0; k < offsets.size(); ++k)
        {
            switch (comps[k](ptr + offsets[k], other.ptr + offsets[k]))
            {
            case -1: return true;
            case  1: return false;
            default: continue;
            }
        };
        return false;
    }
};

bool ParseText(const char* str, std::vector<char>& parsed) noexcept;

template<>
struct TupleView<false> : DataView<false>
{
    static std::vector<size_t> offsets;
    Buffer parsed; //!< parsed is a binary format array

    TupleView(): parsed(parsed_size) {}

    static bool InitParse(const char* format, const std::vector<int>& keys);

    static int non_str_field_number;
    static std::string patched_format;
    static std::vector<Comparer> comps;
    static std::vector<ParseResult> types;
    static std::vector<size_t> keys;
    static size_t parsed_size; //!< size of parsed

    bool ReadFrom(char*& buffer, char* end) noexcept;
    bool operator<(const TupleView& other)const
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            const char *a, *b;
            const auto& k = keys[i];
            const auto& comparer = comps[i];

            if (types[k].type == SCANF_STRING || types[k].type == SCANF_WSTRING)
            {
                a = ptr + *(size_t*)(parsed.data() + offsets[k]);
                b = other.ptr + *(size_t*)(other.parsed.data() + offsets[k]);
            }
            else
            {
                a = parsed.data() + offsets[k];
                b = other.parsed.data() + offsets[k];
            }
            switch (comparer(a, b))
            {
            case -1: return true;
            case  1: return false;
            default: continue;
            }
        };
        return false;
    }
};
