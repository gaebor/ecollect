
#include <stdio.h>
#include <iostream>

#include <vector>

#include "Algorithms.h"
#include "DataTypes.h"
#include "ProgressIndicator.h"

struct Args
{
    size_t binary_size;
    size_t buffer_size;
    int width;

    const char* prefix;
    const char** filenames;

    const char* separators;
    unsigned int seed;

    bool logging, merge, do_delete, async;
    Args() :
        binary_size(0), buffer_size(((size_t)1) << 25), width(3),
        prefix(""), filenames(nullptr), separators("\n\r"),
        seed(0), logging(false), merge(true), do_delete(true), async(false)
    {}
};

template<bool binary>
bool ShuffleMergeFiles(const std::vector<std::string> filenames, bool logging, size_t total, unsigned seed, bool do_delete)
{
    std::vector<FileReader<Packet<DataView<binary>>>> files;
    Packet<DataView<binary>> data;
    for (const auto& filename : filenames)
    {
        files.emplace_back(filename, do_delete);
        if (files.back().f == NULL)
        {
            std::cerr << "Unable to open \"" << filename << "\"!" << std::endl;
            files.pop_back();
        }
    }
    size_t processed = 0;
    MergeShuffle<Packet<DataView<binary>>> shuffler(files.data(), files.data() + files.size(), seed);

    ProgressIndicator(processed, &processed,
        total > 0 ? (total / 100.0) : 1.0,
        total > 0 ? "\rShuffle: %5.1f%% " : "\rShuffle : %.0f ", logging,
        [&](){
        while (shuffler.next(data))
        {
            ++processed;
            data.view.DumpTo(stdout);
        }
    });
    std::cerr << std::endl;
    return true;
}

template<bool binary>
int eshuffle(const Args& args)
{
    SetBinary(args.binary_size);
    SetSeparator(args.separators);

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
        std::vector<DataView<binary>> table;
        std::default_random_engine rng(args.seed);

        result = eprocess<DataView<binary>>(
            args.buffer_size, args.width, args.prefix, args.logging, args.async,
            [&](const DataView<binary>& data)
            {
                table.emplace_back(data);
            },
            [&](size_t)
            {
                std::shuffle(table.begin(), table.end(), rng);
                return std::make_pair(table.data(), table.data() + table.size());
            },
            [&](size_t){ table.clear(); }
            );
        if (result.second == 0)
            return 1;
    }
    if (args.merge)
    {
        return ShuffleMergeFiles<binary>(result.first, args.logging, result.second, args.seed, args.do_delete) ? 0 : 1;
    }
    else
        return 0;
}

int main(int, const char* argv[])
{
    Args args;
    const char* const program_name = argv[0];

    for (++argv; *argv; ++argv)
    {
        if (matches(*argv, { "-b", "--buffer" }) && *(argv + 1))
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
            if (strlen(*++argv) > 0)
                args.separators = *argv;
            else
                std::cerr << "Text separator should not be empty!" << std::endl;
        }
        else if (matches(*argv, { "-l", "--log" }))
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
        else if (matches(*argv, { "--seed", "--random" }) && *(argv + 1))
        {
            args.seed = (unsigned int)atoi(*++argv);
        }
        else if (matches(*argv, { "-h", "--help" }))
        {
            std::cout << " --- External Shuffle --- \n"
                         "     by Gabor Borbely     \n"
                         "  at borbely@math.bme.hu  \n" << std::endl;
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
            std::cout << "\t-w --width <size_t>\ttemporary filename padding width, default " << args.width << std::endl;
            std::cout << "\t-p --prefix <str>\ttemporary filename prefix, default \"" << args.prefix << "\"" << std::endl;
            std::cout << "\t-s --separator <str>\tseparators in text mode, default \"";
            for (const char* c = args.separators; *c; ++c)
                printf("x%02X", *c);
            std::cout << "\"" << std::endl;
            std::cout << "\t-l --log\tincrease verbosity on stderr, default " << args.logging << std::endl;
            std::cout << "\t--binary <size_t>\tspecifies size of data packets in binary mode, default " << args.binary_size << " (bytes)" << std::endl;
            std::cout << "\t-M --no-merge\tdon't merge temporary files just leave them, default " << !args.merge << std::endl;
            std::cout << "\t-m --merge\tdon't collect from stdin rather merge the files specified after this argument, no more argument is parsed" << std::endl;
            std::cout << "\t-D --no-delete\tdon't delete temporary files after merging, default " << !args.do_delete << std::endl;
            std::cout << "\t-a --async\tuses an extra buffer for reading asynchronously from stdin, faster but uses more memory, default " << args.async << std::endl;
            std::cout << "\t--seed --random <int>\tuse this value as random seed, zero means use time, default " << args.seed << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument \"" << *argv << "\"!" << std::endl;
        }
    }

    if (args.seed == 0)
    {
        args.seed = (unsigned int)std::chrono::system_clock::now().time_since_epoch().count();
    }

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
    return (args.binary_size > 0 ? eshuffle<true> : eshuffle<false>)(args);
}
