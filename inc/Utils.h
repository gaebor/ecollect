#pragma once

#include <initializer_list>
#include <string>

bool SetBinaryIO();

bool matches(const char* str, const std::initializer_list<const char*>& patterns);

std::string GetFilename(size_t i, int width = 3, const char* prefix = "");

//! https://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
template <typename T>
inline int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

template <typename T>
inline int cmp(T val1, T val2)
{
    return (val2 < val1) - (val1 < val2);
}

struct Fnv1a
{	// struct for generating FNV-1a hashes
    static constexpr size_t prime = sizeof(size_t) == 8 ? 1099511628211U : 16777619U;
    static constexpr size_t offset = sizeof(size_t) == 8 ? 14695981039346656037U : 2166136261U;
    
    static size_t hash(const char * ptr, size_t size);
    size_t operator()(const char * ptr, size_t size)const;
};

template<typename Type = size_t>
bool blockedmemeq(const void* a, const void* b, size_t size)
{
    bool result = true;
    for (; size >= sizeof(Type); size -= sizeof(Type), ((Type*&)a) += 1, ((Type*&)b) += 1)
        result &= (*(Type*)a == *(Type*)b);
    
    switch (size)
    {
    case 0: return result;
    default: return result && blockedmemeq<char>(a, b, size);
    }
}
