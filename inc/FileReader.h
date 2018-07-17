#pragma once

#include <stdio.h>
#include <vector>
#include <string>

template<typename T>
struct FileReader
{
    FileReader(const std::string& fname, bool del)
        : f(fopen(fname.c_str(), T::binary ? "rb" : "r")), filename(fname),
        do_delete(del)
    {
    }
    bool next(T& t)
    {
        if (t.ReadFrom(f))
            return true;
        else
        {
            Close();
            return false;
        }
    }
    FILE* f;
    const std::string filename;
    const bool do_delete;
private:
    void Close()
    {
        fclose(f);
        f = NULL;
        if (do_delete)
            remove(filename.c_str());
    }
};
