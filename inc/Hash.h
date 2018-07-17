#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

#include "DataTypes.h"

template<bool binary>
class HashTable
{
public:
    size_t actual_size;
    typedef RecordView<binary> value_type;
    typedef DataView<binary> key_type;
private:
    std::vector<value_type> hash_table;
    const double rehash_constant, expand_constant;
    size_t remainder_size;
    
/** @defgroup functions public member functions
*  @{
*/
public:
    HashTable(size_t size, double rehash_factor, double expand_factor)
    :   actual_size(0),
        rehash_constant(rehash_factor), expand_constant(expand_factor)
    {
        hash_table.resize(std::max<size_t>(1, size));
    }
    void rehash()
    {
        value_type *new_bucket;
        const size_t new_table_size = (actual_size < rehash_constant*hash_table.size() ? hash_table.size() : (size_t)ceil(expand_constant*hash_table.size()));
        size_t new_hash_val, i;
        std::vector<value_type> new_table(new_table_size);
        for (const auto& bucket : hash_table)
        {
            if (bucket.ptr)
            {   // non empty bucket
                new_hash_val = Fnv1a::hash(bucket.ptr, bucket.size) % new_table_size;
                i = new_hash_val;
                do
                {
                    new_bucket = new_table.data() + i;
                    if (new_bucket->ptr == nullptr)
                    {   // found an empty bucket
                        *new_bucket = bucket;
                        break;
                    }
                    i = (i + 1) % new_table_size; // circular try
                } while (i != new_hash_val);
            }
        }
        std::swap(hash_table, new_table);
    }

    void insert(const key_type& str)
    {
        const size_t supposed_to_be = Fnv1a::hash(str.ptr, str.size) % hash_table.size();
        value_type* where;
        size_t i = supposed_to_be;
        do
        {
            where = hash_table.data() + i;
            if (where->ptr == nullptr)
            {
                static_cast<key_type&>(*where) = str;
                where->count = 1;
                ++actual_size;
                return;
            }
            else if (*where == str)
            {
                ++(where->count);
                return;
            }
            i = (i + 1) % hash_table.size();
        } while (i != supposed_to_be); // cyclic try until it goes a full circle
    }

    void SortFreqDescent()
    {
        static struct 
        {
            bool operator()(const value_type& one, const value_type& other)const
            {
                return one.count > other.count;
            }
        } less;
        std::sort(hash_table.begin(), hash_table.end(), less);
    }

    void SortLexicographic(size_t from = 0)
    {
        std::sort(hash_table.begin() + from, hash_table.begin() + actual_size);
    }

    size_t GetAllocatedSize()const { return hash_table.size(); }
    size_t GetSize()const { return actual_size; }
    value_type* GetTable(){ return hash_table.data(); }
    const value_type* GetTable()const { return hash_table.data(); }

/** @} */
};
