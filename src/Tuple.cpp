#include "Tuple.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <algorithm>
#include <vector>
#include <cstdarg>

std::vector<Comparer> TupleView<false>::comps;
size_t TupleView<false>::parsed_size = 0;
int TupleView<false>::non_str_field_number = 0;
std::vector<size_t> TupleView<false>::offsets;
std::vector<size_t> TupleView<false>::keys;
std::vector<ParseResult> TupleView<false>::types;
std::string TupleView<false>::patched_format;

std::vector<size_t> TupleView<true>::offsets;
std::vector<Comparer> TupleView<true>::comps;

static int size_of(FormatType type)
{
    switch (type)
    {
    case SCANF_STRING:
    case SCANF_WSTRING: return sizeof(size_t); // a %zn field is put before it
    case SCANF_CHAR: return sizeof(char);
    case SCANF_WCHAR: return sizeof(wchar_t);
    case SCANF_UCHAR: return sizeof(unsigned char);
    case SCANF_SHORT: return sizeof(short int);
    case SCANF_USHORT: return sizeof(unsigned short int);
    case SCANF_INT: return sizeof(int);
    case SCANF_UINT: return sizeof(unsigned int);
    case SCANF_LONG: return sizeof(long int);
    case SCANF_LLONG: return sizeof(long long int);
    case SCANF_ULONG: return sizeof(unsigned long int);
    case SCANF_ULLONG: return sizeof(unsigned long long int);
    case SCANF_SIZET: return sizeof(size_t);
    case SCANF_PTR: return sizeof(void*);
    case SCANF_FLOAT: return sizeof(float);
    case SCANF_DOUBLE: return sizeof(double);
    case SCANF_LDOUBLE: return sizeof(long double);
    case SCANF_UNKNOWN:
    default: return -1;
    };
}

static size_t constexpr LengthId(const char length_str[3])
{
    return ((unsigned char)(length_str[2]) * std::numeric_limits<unsigned char>::max() +
        (unsigned char)(length_str[1])) * std::numeric_limits<unsigned char>::max() +
        (unsigned char)(length_str[0]);
}

static std::vector<ParseResult> Parse(const char * format)
{
    std::vector<ParseResult> results;
    size_t pos = 0;
    const char* fmt_pos;
    while (*format)
    {
        fmt_pos = format; // remember place
        if (*format == '%')
        {
            ++format;
            if (*format == '%') // escaped %
            {
                ++pos;
                ++format;
            }
            else
            {
                bool skipped = false;
                if (*format == '*')
                {
                    skipped = true;
                    ++format;
                }
                FormatType type = ParseType(format);
                if (type != SCANF_UNKNOWN)
                {
                    results.emplace_back();
                    results.back().pos = pos;
                    results.back().type = type;
                    results.back().skipped = skipped;
                    pos += size_of(type);
                }
                else
                {
                    std::cerr << "Skipping un-parsable format \"" << std::string(fmt_pos, format) << "\"!" << std::endl;
                }
            }
        }
        else
        {   // a non-formatting character
            ++pos;
            ++format;
        }
    }
    return results;
}

bool TupleView<true>::InitParse(const char* format, const std::vector<int>& keys)
{
    auto parsed_types = Parse(format);
    {
        auto begin = parsed_types.begin();
        while (begin != parsed_types.end())
        {
            if (begin->type == SCANF_STRING || begin->type == SCANF_WSTRING)
            {
                std::cerr << "String field is not permitted for binary input!" << std::endl;
                return false;
            }
            if (begin->skipped)
                begin = parsed_types.erase(begin);
            else
                ++begin;
        }
    }
    size_t required_size = 0;
    if (!parsed_types.empty())
    {
        required_size = parsed_types.back().pos + size_of(parsed_types.back().type);
    }
    if (size < required_size)
    {
        std::cerr << "Binary size is less than the size deduced from the format string! " << size << " < " << required_size << std::endl;
        return false;
    }

    offsets.clear();
    comps.clear();
    for (auto kk : keys)
    {
        const bool reversed = kk < 0;
        const size_t k = std::abs(kk) - 1;
        if (k < parsed_types.size())
        {
            offsets.emplace_back(parsed_types[k].pos);
            comps.emplace_back(MakeComparer(parsed_types[k].type, reversed));
        }
        else
        {
            std::cerr << "Key " << k+1 << " is out of range of parsed fields!" << std::endl;
            return false;
        }
    }
    return true;
}

static std::string PatchFormat(const char* format)
{   // replaces non-skipped string types with %zn and then the same string but skipped
    // %80[^\t] ->%zn%*80[^\t]
    std::string patched_format;
    const char* fmt_pos;
    while (*format)
    {
        fmt_pos = format; // remember place
        if (*format == '%')
        {
            ++format;
            if (*format == '%') // escaped %
            {
                ++format;
                patched_format += "%%";
            }
            else
            {
                bool skipped = false;
                if (*format == '*')
                {
                    skipped = true;
                    ++format;
                }
                FormatType type = ParseType(format);
                // Skipping un - parsable format
                if (type != SCANF_UNKNOWN)
                {
                    const std::string this_format(fmt_pos, format);
                    if (!skipped && (type == SCANF_STRING || type == SCANF_WSTRING))
                    {   // patch
                        patched_format += "%zn%*";
                        patched_format += this_format.c_str() + 1;
                    }
                    else
                        patched_format += this_format;
                }
            }
        }
        else
        {   // a non-formatting character
            patched_format += *format;
            ++format;
        }
    }
    return patched_format;
}

bool TupleView<false>::InitParse(const char* format, const std::vector<int>& signed_keys)
{
    non_str_field_number = 0;
    parsed_size = 0;
    
    types = Parse(format);
    patched_format = PatchFormat(format);
    offsets.clear();

    for (auto& type : types)
    {
        if (!type.skipped && type.type != SCANF_STRING && type.type != SCANF_WSTRING)
            ++non_str_field_number;
    }
    {
        size_t offset = 0;
        const auto patched_types = Parse(patched_format.c_str());
        for (auto& type : patched_types)
            if (!type.skipped)
            {
                parsed_size += size_of(type.type);
                offsets.emplace_back(offset);
                offset += size_of(type.type);
            }
    }

    comps.clear();
    keys.clear();
    for (auto kk : signed_keys)
    {
        const bool reversed = kk < 0;
        const size_t k = std::abs(kk) - 1;
        if (k < types.size())
        {
            comps.emplace_back(MakeComparer(types[k].type, reversed));
            keys.emplace_back(k);
            //types.emplace_back(raw_types[k]);
            //offsets.emplace_back(raw_offsets[k]);
        }
        else
        {
            std::cerr << "Key " << k + 1 << " is out of range of parsed fields!" << std::endl;
            return false;
        }
    }
    return true;
}

static FormatType GetType(size_t lengthid, char specifier)
{
    switch (specifier)
    {
    case 'd':
    case 'i':
    case 'n':
        switch (lengthid)
        {
        case LengthId("hh"):
            return SCANF_CHAR;
        case LengthId("h\0"):
            return SCANF_SHORT;
        case LengthId("\0\0"):
            return SCANF_INT;
        case LengthId("l\0"):
            return SCANF_LONG;
        case LengthId("ll\0"):
            return SCANF_LLONG;
        case LengthId("z\0"):
            return SCANF_SIZET;
        default:
            return SCANF_UNKNOWN;
        };
    case 'u':
    case 'o':
    case 'x':
        switch (lengthid)
        {
        case LengthId("hh"):
            return SCANF_UCHAR;
        case LengthId("h\0"):
            return SCANF_USHORT;
        case LengthId("\0\0"):
            return SCANF_UINT;
        case LengthId("l\0"):
            return SCANF_ULONG;
        case LengthId("ll\0"):
            return SCANF_ULLONG;
        case LengthId("z\0"):
            return SCANF_SIZET;
        default:
            return SCANF_UNKNOWN;
        };
    case 'f':
    case 'e':
    case 'g':
        switch (lengthid)
        {
        case LengthId("\0\0"):
            return SCANF_FLOAT;
        case LengthId("l\0"):
            return SCANF_DOUBLE;
        case LengthId("L\0"):
            return SCANF_LDOUBLE;
        default:
            return SCANF_UNKNOWN;
        };
    case 'c':
        switch (lengthid)
        {
        case LengthId("\0\0"):
            return SCANF_CHAR;
        case LengthId("l\0"):
            return SCANF_WCHAR;
        default:
            return SCANF_UNKNOWN;
        };
    case 's':
    case '[':
        switch (lengthid)
        {
        case LengthId("\0\0"):
            return SCANF_STRING;
        case LengthId("l\0"):
            return SCANF_WSTRING;
        default:
            return SCANF_UNKNOWN;
        };
    case 'p':
        switch (lengthid)
        {
        case LengthId("\0\0"):
            return SCANF_PTR;
        default:
            return SCANF_UNKNOWN;
        };
    default: return SCANF_UNKNOWN;
    };
}

// http://www.cplusplus.com/reference/cstdio/scanf/
FormatType ParseType(const char *& fmt)
{
    char length_str[] = { '\0', '\0', '\0' };
    int n;
    unsigned int width = 0;

    if (sscanf(fmt, "%u%n", &width, &n) > 0)
        fmt += n;
    if (sscanf(fmt, "%2[hlzL]%n", length_str, &n) > 0)
        fmt += n;

    auto type = GetType(LengthId(length_str), fmt[0]);
    if (fmt++[0] == '[')
    {   // read until ]
        // https://stackoverflow.com/questions/5248397/scanf-formatting-issue
        if (*fmt == '^')
            ++fmt;
        
        ++fmt; // one character is mandatory;
        while (*fmt != '\0' && *fmt != ']')
            ++fmt;

        if (*fmt != ']')
            return SCANF_UNKNOWN;
        else
            ++fmt;
    }
    return type;
}

Comparer MakeComparer(FormatType type, bool reverse)
{
    return reverse ? MakeComparer<true>(type) : MakeComparer<false>(type);
}

bool ParseText(const char* str, std::vector<char>& parsed) noexcept
{
    const auto& offsets = TupleView<false>::offsets;
    const auto& fmt = TupleView<false>::patched_format.c_str();

    int result = 0;
    switch (offsets.size())
    {
    case 0: result = sscanf(str, fmt); break;
    case 1: result = sscanf(str, fmt, parsed.data() + offsets[0]); break;
    case 2: result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1]); break;
    case 3: result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1],
                                      parsed.data() + offsets[2]); break;
    case 4: result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1],
                                      parsed.data() + offsets[2],
                                      parsed.data() + offsets[3]); break;
    case 5: result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1],
                                      parsed.data() + offsets[2],
                                      parsed.data() + offsets[3],
                                      parsed.data() + offsets[4]); break;
    case 6: result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1],
                                      parsed.data() + offsets[2],
                                      parsed.data() + offsets[3],
                                      parsed.data() + offsets[4],
                                      parsed.data() + offsets[5]); break;
    case 7: result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1],
                                      parsed.data() + offsets[2],
                                      parsed.data() + offsets[3],
                                      parsed.data() + offsets[4],
                                      parsed.data() + offsets[5],
                                      parsed.data() + offsets[6]); break;
    case 8: result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1],
                                      parsed.data() + offsets[2],
                                      parsed.data() + offsets[3],
                                      parsed.data() + offsets[4],
                                      parsed.data() + offsets[5],
                                      parsed.data() + offsets[6],
                                      parsed.data() + offsets[7]); break;
    case 9: result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1],
                                      parsed.data() + offsets[2],
                                      parsed.data() + offsets[3],
                                      parsed.data() + offsets[4],
                                      parsed.data() + offsets[5],
                                      parsed.data() + offsets[6],
                                      parsed.data() + offsets[7],
                                      parsed.data() + offsets[8]); break;
    case 10:result = sscanf(str, fmt, parsed.data() + offsets[0],
                                      parsed.data() + offsets[1],
                                      parsed.data() + offsets[2],
                                      parsed.data() + offsets[3],
                                      parsed.data() + offsets[4],
                                      parsed.data() + offsets[5],
                                      parsed.data() + offsets[6],
                                      parsed.data() + offsets[7],
                                      parsed.data() + offsets[8],
                                      parsed.data() + offsets[9]); break;
    default:
        std::cerr << "You cannot have " << offsets.size() << " fields in the text format!" << std::endl;
        std::cerr << "You can remedy this situation by extending the switch cases in " << __FUNCTION__ << std::endl;
        exit(1);
    };

    return result == TupleView<false>::non_str_field_number;

    /*
        Oh, boy, I tried, tried and failed!
    */
    //static std::vector<void*> vaargs;
    //vaargs.clear();
    //for (auto i : TupleView<false>::offsets)
    //    vaargs.emplace_back(parsed.data() + i);
    //void* arg_start = vaargs[0];
    //va_list vaarg;
    //va_start(vaarg, arg_start);

    //int result = vsscanf(str, (const char*)(vaargs[0]), vaarg);
    //va_end(vaarg);
    //return result == TupleView<false>::non_str_field_number;

    // result = vsscanf(str, fmt, (va_list)vaargs.data());
}

template<>
bool Packet<TupleView<true>>::ReadFrom(FILE* f)
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

bool TupleView<false>::ReadFrom(char*& buffer, char* end) noexcept
{
    return (DataView<false>::ReadFrom(buffer, end)) &&
        ParseText(ptr, parsed);
}

template<>
bool Packet<TupleView<false>>::ReadFrom(FILE* f)
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
    return ParseText(view.ptr, view.parsed);
    // return c != EOF || size() > 1;
}
