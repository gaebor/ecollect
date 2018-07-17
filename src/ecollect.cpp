
#include <stdio.h>
#include <iostream>

#include <vector>
#include <algorithm>

#include "Algorithms.h"
#include "DataTypes.h"
#include "Hash.h"
#include "ProgressIndicator.h"

struct Args
{
    double rehash_constant, expand_constant, keep_factor;
    
    size_t binary_size, buffer_size;
    int width;

    const char* prefix;
    const char** filenames;
    std::string separators;
    bool logging, merge, do_delete, async;

    Args() : 
        rehash_constant(0.75), expand_constant(2.0), keep_factor(0.5),
        binary_size(0), buffer_size(((size_t)1) << 25), width(3),
        prefix(""), filenames(nullptr), separators("\t\n\v\f\r"),
        logging(false), merge(true), do_delete(true), async(false)
    {}
};

template<bool binary, bool pre_sorted>
std::pair<size_t, size_t> sum_up_lengths(const HashTable<binary>& hash_table, size_t buffer_size)
{
    std::pair<size_t, size_t> remain(0, 0);
    if (binary)
    {
        remain.first = buffer_size / hash_table.GetTable()->size;
        remain.second = hash_table.GetSize() * hash_table.GetTable()->size;
    }
    else
    {
        if (pre_sorted)
        {
            auto end = hash_table.GetTable() + hash_table.GetSize();
            for (auto rec = hash_table.GetTable(); rec < end; ++rec)
            {
                if ((remain.second += rec->size) <= buffer_size)
                    ++remain.first;
                else 
                    break;
            }
        }
        else
        {
            auto end = hash_table.GetTable() + hash_table.GetAllocatedSize();
            for (auto rec = hash_table.GetTable(); rec < end; ++rec)
            {
                if (rec->ptr)
                {
                    if ((remain.second += rec->size) <= buffer_size)
                        ++remain.first;
                }
            }
        }
    }
    return remain;
}

//! put data ('remain' number of records) into memory-buffer
template<bool binary>
void reorder_data(HashTable<binary>& hash_table, std::vector<char>& memory, size_t buffer_size, size_t remain)
{
    std::vector<char> temp(buffer_size);
    char* place = temp.data();
    size_t done = 0;
    const auto end = hash_table.GetTable() + hash_table.GetAllocatedSize();
    for (auto rec = hash_table.GetTable(); done < remain && rec < end; ++rec)
    {
        if (rec->ptr)
        {
            memcpy(place, rec->ptr, rec->size);
            rec->ptr = place;
            place += rec->size;
            ++done;
        }
    }
    std::swap(temp, memory);
}

template<bool binary>
bool MergeFiles(const std::vector<std::string>& filenames, bool logging, size_t total, bool do_delete)
{
    std::vector<FileReader<Packet<RecordView<binary>>>> files;
    for (const auto& filename : filenames)
    {
        files.emplace_back(filename, do_delete);
        if (files.back().f == NULL)
        {
            std::cerr << "Unable to open \"" << filename << "\"!" << std::endl;
            files.pop_back();
        }
    }
    MergeSort<Packet<RecordView<binary>>> sorter(files.data(), files.data() + files.size());
    Packet<RecordView<binary>> previous;
    if (sorter.next(previous))
    {
        size_t processed = 0;
        Packet<RecordView<binary>> next;
        std::string format_str = total > 0 ? "\rMerging: %5.1f%% " : "\rMerging: %.0f ";
        if (!binary)
            format_str += "\"% .30s\"     ";
        ProgressIndicator(processed, &processed,
            total > 0 ? (total / 100.0) : 1.0,
            format_str.c_str(), logging,
            [&]() {
            while (sorter.next(next))
            {
                ++processed;
                if (previous.view == next.view)
                    previous.view.count += next.view.count;
                else
                {
                    previous.view.DumpTo(stdout);
                    previous = std::move(next);
                }
            }
            ++processed;
            previous.view.DumpTo(stdout);
        }, &previous.view.ptr);
    }
    std::cerr << std::endl;
    return true;
}

template<bool binary>
int ecollect(const Args& args)
{
    SetBinary(args.binary_size);
    SetSeparator(args.separators.c_str());

    size_t total_dumped = 0;
    std::pair<std::vector<std::string>, size_t> result;
    if (args.filenames)
    {   // shuffle merge these files
        for (auto filename = args.filenames; *filename; ++filename)
        {
            result.first.emplace_back(*filename);
        }
    }
    else
    {   // collect from stdin
        HashTable<binary> hash_table(8, args.rehash_constant, args.expand_constant);
        
        std::vector<char> in_memory;
        // first index of first element to dump
        // second total bytes in buffer
        std::pair<size_t, size_t> remain;

        result = eprocess<DataView<binary>>(
            args.buffer_size, args.width, args.prefix, args.logging, args.async,
            [&](const DataView<binary>& data)
            {
                hash_table.insert(data);
                if (hash_table.GetSize() > args.rehash_constant*hash_table.GetAllocatedSize())
                    hash_table.rehash();
            },
            [&](size_t buffer_size)
            {
                remain = sum_up_lengths<binary, false>(hash_table, buffer_size);
                fprintf(stderr,
                    buffer_size > 0 ? ", Buffer: %5.1f%%" : "Buffer: %5.1f%%",
                    (100.0*remain.second) / args.buffer_size);
                if (remain.second > buffer_size)
                {
                    // keep as much of the frequent ones as possible
                    hash_table.SortFreqDescent();
                    remain = sum_up_lengths<binary, true>(hash_table, buffer_size);
                    remain.first = (size_t)std::floor(remain.first * args.keep_factor);
                    hash_table.SortLexicographic(remain.first);
                }
                return std::make_pair(hash_table.GetTable() + remain.first, hash_table.GetTable() + hash_table.GetSize());
            },
            [&](size_t dumped)
            {
                total_dumped += dumped;
                // clear dumped
                std::fill_n(hash_table.GetTable() + remain.first, dumped, RecordView<binary>());
                hash_table.actual_size -= dumped;
                // move remaining data in-memory
                reorder_data<binary>(hash_table, in_memory, args.buffer_size, remain.first);
                // rehash remaining
                hash_table.rehash();
            }
            );
        if (result.second == 0)
            return 1;
    }
    if (args.merge)
        return MergeFiles<binary>(result.first, args.logging, total_dumped, args.do_delete) ? 0 : 1;
    else
        return 0;
}

int main(int, const char* argv[])
{
    Args args;
    const char* const program_name = argv[0];

    for (++argv; *argv; ++argv)
    {
        if (matches(*argv, { "-r", "--rehash" }) && *(argv + 1))
        {
            const auto tmp = atof(*++argv);
            if (tmp < 1.0 && tmp > 0.0)
                args.rehash_constant = tmp;
            else
            {
                std::cerr << "\"rehash factor\" should be between 0 and 1 !" << std::endl;
                return 1;
            }
        }
        else if (matches(*argv, { "-e", "--expand" }) && *(argv + 1))
        {
            const auto tmp = atof(*++argv);
            if (tmp > 1.0)
                args.expand_constant = tmp;
            else
            {
                std::cerr << "\"expand factor\" should be between greater than 1 !" << std::endl;
                return 1;
            }
        }
        else if (matches(*argv, { "-k", "--keep" }) && *(argv + 1))
        {
            const auto tmp = atof(*++argv);
            if (tmp < 1.0 && tmp > 0.0)
                args.keep_factor = tmp;
            else
            {
                std::cerr << "\"keep factor\" should be between 0 and 1 !" << std::endl;
                return 1;
            }
        }
        else if (matches(*argv, { "-b", "--buffer" }) && *(argv + 1))
        {
            args.buffer_size = (size_t)std::max(atoll("1"), atoll(*++argv));
        }
        else if (matches(*argv, { "-w", "--width" }) && *(argv + 1))
        {
            args.width = std::max(1, atoi(*++argv));
        }
        else if (matches(*argv, { "-p", "--prefix" }) && *(argv + 1))
        {
            args.prefix = *++argv;
        }
        else if (matches(*argv, { "-s", "--separator", "--separators" }) && *(argv + 1))
        {
            ++argv;
            if (strchr(*argv, '\n'))
                std::cerr << "new line is included by default as a separator!" << std::endl;
            args.separators = *argv;
        }
        else if (matches(*argv, { "-l", "--log"}))
        {
            args.logging = true;
        }
        else if (matches(*argv, { "--binary" }) && *(argv + 1))
        {
            args.binary_size = std::max(1, atoi(*++argv));
        }
        else if (matches(*argv, { "-M", "--no-merge" }))
        {
            args.merge = false;
        }
        else if (matches(*argv, { "-m", "--merge" }))
        {
            args.filenames = ++argv;
            args.merge = true;
            break; // no more arguments
        }
        else if (matches(*argv, { "-D", "--no-delete" }))
        {
            args.do_delete = false;
        }
        else if (matches(*argv, { "-a", "--async" }))
        {
            args.async = true;
        }
        else if (matches(*argv, { "-h", "--help" }))
        {
            std::cout << std::boolalpha;
            std::cout << " --- External Collect --- \n"
                         "     by Gabor Borbely     \n"
                         "  at borbely@math.bme.hu  \n"<< std::endl;
            std::cout <<
                "  _____                      \n"
                " |A .  | _____               \n"
                " | /.\\ ||A ^  | _____        \n"
                " |(_._)|| / \\ ||A _  | _____ \n"
                " |  |  || \\ / || ( ) ||A_ _ |\n"
                " |____V||  .  ||(_'_)||( v )|\n"
                "        |____V||  |  || \\ / |\n"
                "               |____V||  .  |\n"
                "                      |____V|\n"
                "ascii art by ejm\n" << std::endl;
            std::cout << "USAGE: " << program_name << " [options] < input.file > output.result" << std::endl;
            std::cout << "OPTIONS:" << std::endl;
            std::cout << "\t-h --help\tshow this help and exit" << std::endl;
            std::cout << "\t-r --rehash <double>\trehash factor for hash table, default " << args.rehash_constant << std::endl;
            std::cout << "\t-e --expand <double>\texpansion rate for hash table, default " << args.expand_constant << std::endl;
            std::cout << "\t-k --keep <double>\tkeep factor, sets how much to keep in memory in case of dumping into file, default " << args.keep_factor << std::endl;
            std::cout << "\t-w --width <size_t>\ttemporary filename padding width, default " << args.width << std::endl;
            std::cout << "\t-p --prefix <str>\ttemporary filename prefix, default \"" << args.prefix << "\"" << std::endl;
            std::cout << "\t-s --separator <str>\tseparators in text mode, default \"";
            for (auto c : args.separators)
                printf("x%02X", c);
            std::cout << "\" (newline will be included by default)" << std::endl;
            std::cout << "\t-l --log\tincrease verbosity on stderr, default " << args.logging << std::endl;
            std::cout << "\t--binary <size_t>\tspecifies size of data packets in binary mode, default " << args.binary_size << " (bytes)"<< std::endl;
            std::cout << "\t-M --no-merge\tdon't merge temporary files just leave them, default " << !args.merge << std::endl;
            std::cout << "\t-m --merge\tdon't collect from stdin rather merge the files specified after this argument, no more argument is parsed" << std::endl;
            std::cout << "\t-D --no-delete\tdon't delete temporary files after merging, default " << !args.do_delete << std::endl;
            std::cout << "\t-a --async\tuses an extra buffer for reading asynchronously from stdin, faster but uses more memory, default " << args.async << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument \"" << *argv << "\"!" << std::endl;
        }
    }
    
    if (args.separators.find('\n') == std::string::npos)
        args.separators += '\n';

    if (args.binary_size > 0)
    {
        if (args.buffer_size % args.binary_size != 0)
        {
            std::cerr << "Buffer size (" << args.buffer_size << ") should be divisible by binary data size (" << args.binary_size << ")!" << std::endl;
            return 1;
        }
        if (!SetBinaryIO())
        {
            std::cerr << "Unable to switch stdin and stdout to binary!" << std::endl;
            return 1;
        }
    }
    return (args.binary_size > 0 ? ecollect<true> : ecollect<false>)(args);
}
