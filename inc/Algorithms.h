#pragma once

#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <random>
#include <future>
#include <utility>

#include "Utils.h"
#include "FileReader.h"

template<typename T>
size_t Dump(const T* begin, const T* end, const std::string& filename)
{
    size_t written = 0;
    if (begin < end)
    {
        FILE* f = filename.empty() ? stdout : fopen(filename.c_str(), T::binary ? "wb" : "w");
        if (f)
        {
            while(begin < end)
            {
                if (begin->DumpTo(f))
                {
                    ++written;
                    ++begin;
                }
                else
                    break;
            }
            fclose(f);
        }
    }
    return written;
}

template<typename T, typename Accumulator, typename Dumper, typename DumpCallback>
std::pair<std::vector<std::string>, size_t>
eprocess(
    size_t buffer_size, int width, const char* prefix, bool logging, bool async,
    Accumulator accumulator, Dumper dumper, DumpCallback dump_callback)
{
    {   //test
        std::function<void(const T&)> accumulator_f = accumulator;
        std::function<std::pair<const T*, const T*>(size_t)> dumper_f = dumper;
        std::function<void(size_t)> callback_f = dump_callback;
    }
    std::pair<std::vector<std::string>, size_t> result;
    auto& dumped_total = result.second; // successfully dumped elements
    dumped_total = 0;
    auto& filenames = result.first;
    std::vector<char> buffer(buffer_size);
    std::vector<char> async_buffer;
    std::future<void> reading;
    auto reader = [&]()
    {
        async_buffer.resize(buffer_size);
        async_buffer.resize(fread(async_buffer.data(), 1, buffer_size, stdin));
    };

    T t;
    size_t unprocessed = 0; // left-overs from earlier
    size_t processed = 0; // total number of bytes processed so far
    size_t dumped;

    if (async)
    {
        reading = std::async(std::launch::async, reader);
    }

    while (true)
    {
        std::cerr << "\rReading... ";
        if (async)
        {
            reading.get();
            memcpy(buffer.data() + unprocessed, async_buffer.data(), async_buffer.size());
            buffer.resize(unprocessed + async_buffer.size());
            reading = std::async(std::launch::async, reader);
        }
        else
        {
            buffer.resize(unprocessed + fread(buffer.data() + unprocessed, 1, buffer_size, stdin));
        }
        // finish when you cannot read any more data
        if (buffer.empty())
            break;
        char* buffer_state = buffer.data();
        char* const buffer_end = buffer.data() + buffer.size();
        ProgressIndicator((std::ptrdiff_t)buffer_state - processed, &buffer_state,
            1.0, "\rProcessed: %.0f bytes", logging,
            [&]() {
            while (t.ReadFrom(buffer_state, buffer_end))
            {
                accumulator(t);
            }
        });
        processed += std::distance(buffer.data(), buffer_state);

        // dump if necessary
        dumped = 0;
        const auto to_dump = dumper(buffer_size);
        const auto filename = GetFilename(filenames.size() + 1, width, prefix);
        if (to_dump.first < to_dump.second)
        {
            std::cerr << " -> " << filename;
            dumped = Dump(to_dump.first, to_dump.second, filename);
            if (dumped > 0)
            {
                filenames.push_back(filename);
                std::cerr << std::endl;
                dumped_total += dumped;
            }
            else
            {
                std::cerr << " Failed!" << std::endl;
                result.second = 0;
                return result;
            }
        }
        
        dump_callback(dumped);

        // empty buffer and prepare for next batch
        unprocessed = std::distance(buffer_state, buffer_end);
        memcpy(buffer.data(), buffer_state, unprocessed);
        buffer.resize(unprocessed + buffer_size);
    }
    dumped = 0;
    if (filenames.empty())
        std::cerr << std::endl;
    const auto to_dump = dumper(0); // empty everything
    if (to_dump.first < to_dump.second)
    {
        if (filenames.empty())
        {   // no need to write in file, because there is nothing to merge with
            dumped = Dump(to_dump.first, to_dump.second, "");
            dumped_total += dumped;
        }
        else
        {
            const auto filename = GetFilename(0, width, prefix);
            std::cerr << " -> " << filename;
            dumped = Dump(to_dump.first, to_dump.second, filename);
            if (dumped > 0)
            {
                filenames.push_back(filename);
            }
            else
            {
                std::cerr << " Failed!" << std::endl;
                result.second = 0;
                return result;
            }
            dumped_total += dumped;
        }
    }
    std::cerr << std::endl;
    dump_callback(dumped);
    return result;
}

template<typename T, typename Comp = std::less<T>>
class MergeSort
{
public:
    typedef FileReader<T>* ReaderType;
private:
    typedef std::pair<T*, ReaderType> Pair;
    // typedef std::unique_ptr<Pair> PairPtr;
public:
    MergeSort(ReaderType begin, ReaderType end, Comp comparer = Comp())
        : comp(comparer)
    {
        for (; begin < end; ++begin)
        {
            queue.emplace_back(new T(), begin);
            auto& p = queue.back();
            if (p.second->next(*p.first))
            {
                //queue.emplace_back( new Pair(t, begin));
                std::push_heap(queue.begin(), queue.end(), comp);
            }
            else
            {
                delete p.first;
                queue.pop_back();
            }
                
        }
    }
    bool next(T& t)
    {
        if (queue.empty())
            return false;
        // pop-heap
        std::pop_heap(queue.begin(), queue.end(), comp);
        // top is at back() now
        Pair& top = queue.back();
        // retrieve top
        t = std::move(*top.first);
        // try to retrieve next from the very same FileReader who produced the previous top
        if (top.second->next(*top.first))
        {   // re-heap with the newly read element
            std::push_heap(queue.begin(), queue.end(), comp);
        }
        else 
        {   // remove the exhausted FileReader
            delete top.first;
            queue.pop_back();
        }
        return true;
    }
private:
    struct grt
    {
        grt(Comp comparer) : comp(comparer) {}
        bool operator()(const Pair& one, const Pair& other)const
        {
            return comp(*other.first, *one.first); // actually smallest is the greatest priority
        }
        const Comp comp;
    } comp;
    std::vector<Pair> queue;
};

template<typename T>
class MergeShuffle
{
public:
    typedef FileReader<T>* ReaderType;

    MergeShuffle(ReaderType begin, ReaderType end, unsigned seed)
        : queue(), generator(seed)
    {
        while(begin < end)
            queue.emplace_back(begin++);
    }
    bool next(T& t)
    {
        while (!queue.empty())
        {
            auto where = queue.begin();
            std::advance(where, generator() % queue.size());
            if ((*where)->next(t))
                return true;
            else
                queue.erase(where);
        }
        return false;
    }
private:
    std::vector<ReaderType> queue;
    std::default_random_engine generator;
};
