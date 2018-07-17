#pragma once

#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

template<bool binary>
struct DataView;

template<>
struct DataView<true>
{
    const char* ptr;

    DataView(): ptr(nullptr){}
    inline bool ReadFrom(char*& buffer, char* end)noexcept
    {
        if (buffer + size <= end)
        {
            ptr = buffer;
            buffer += size;
            return true;
        }
        else
        {
            ptr = nullptr;
            return false;
        }
    }
    inline bool DumpTo(FILE* f)const noexcept
    {
        return fwrite(ptr, size, 1, f) == 1;
    }
    inline bool operator<(const DataView& other)const noexcept
    {
        return memcmp(ptr, other.ptr, size) < 0;
    }

    inline bool operator==(const DataView& other)const noexcept
    {
        return memcmp(ptr, other.ptr, size) == 0;
    }

    static size_t size;
    static constexpr bool binary = true;
};

template<>
struct DataView<false>
{
    const char* ptr;
    size_t size;
    
    DataView() : ptr(nullptr), size(0) {}
    inline bool ReadFrom(char*& buffer, char* end)noexcept
    {
        while (buffer < end && strchr(separators, *buffer) != NULL)
            *buffer++ = '\0';
        ptr = buffer;
        while (buffer < end && strchr(separators, *buffer) == NULL)
            ++buffer;
        if (buffer == end)
        {   // no separator at the end, setting back to last known place
            size = 0;
            buffer = (char*)ptr;
            ptr = nullptr;
            return false;
        }
        else
        {
            *buffer++ = '\0';
            size = buffer - ptr;
            return true;
        }
    }

    inline bool DumpTo(FILE* f)const noexcept
    {
        //return fprintf(f, "%s%c", ptr, separators[0]) > 0;
        return fwrite(ptr, size - 1, 1, f) == 1 && fputc(separators[0], f) != EOF;
    }

    inline bool operator<(const DataView& other)const
    {
        return strcmp(ptr, other.ptr) < 0;
    }
    inline bool operator==(const DataView& other)const
    {
        return size == other.size &&
            memcmp(ptr, other.ptr, size) == 0;
    }
    
    static const char* separators;
    static constexpr bool binary = false;
};

typedef std::vector<char> Buffer;

template<bool binary>
struct RecordView : DataView<binary>
{
    // char* ReadFrom(char*& buffer, char* end);
    inline bool DumpTo(FILE* f)const noexcept;

    size_t count;
    static char separator;
    // inherits lexicographic operator< from DataView
    // inherits operator== from DataView
};

template<bool binary>
char RecordView<binary>::separator = '\0';

void SetSeparator(const char* sep);
void SetBinary(size_t binary_size);

template<typename T>
struct Packet : private Buffer
{
    static constexpr bool binary = T::binary;

    Packet() : Buffer(), view() {}
private:
    Packet(const Packet& other)
        : Buffer(other), view(other.view)
    {
        view.ptr = data();
    }

    Packet& operator=(const Packet& other)
    {
        static_cast<Buffer&>(*this) = other;
        view = other.view;
        view.ptr = data();
        return *this;
    }
public:
    //! move
    Packet& operator=(Packet&& other)
    {
        std::swap(static_cast<Buffer&>(*this), static_cast<Buffer&>(other));
        std::swap(view, other.view);
        return *this;
    }

    inline bool operator<(const Packet& other)const
    {
        return view < other.view;
    }

    bool ReadFrom(FILE* f);
    T view;
};

template<>
inline bool RecordView<true>::DumpTo(FILE * f) const noexcept
{
    return fwrite(this->ptr, this->size, 1, f) == 1 && fwrite(&count, sizeof(count), 1, f) == 1;
}

template<>
inline bool RecordView<false>::DumpTo(FILE * f) const noexcept
{
    return fwrite(ptr, size - 1, 1, f) == 1 && fputc(separator, f) != EOF &&
        fprintf(f,
#if defined(_MSC_VER) && _MSC_VER < 1900 
        "%Iu\n"
#else
        "%zu\n"
#endif
        , count) > 0;
}
