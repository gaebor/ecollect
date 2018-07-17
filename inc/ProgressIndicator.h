#pragma once

#include <thread>
#include <chrono>
#include <cstdio>
#include <exception>

template<class Exception = std::exception, typename T1, typename T2, typename Func, typename ...Args>
void ProgressIndicator(T1 begin, T2* p, double total, const char* fmt,
                        bool enable, const Func& f, Args*... args)
{
    bool run = true;
    std::thread progress;
    int delay = 10;
    if (enable)
    {
        progress = std::thread([&]()
        {
            while (run)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                delay = std::min(1000, delay*10);
                if (run)
                {
                    fprintf(stderr, fmt, size_t(*p - begin) / total, *args...);
                    fflush(stderr);
                }
            }
        });
    }

    try
    {
        f();
    }
    catch (const Exception& e)
    {
        run = false;
        if (progress.joinable())
            progress.join();

        throw e;
    }
    run = false;
    if (progress.joinable())
        progress.join();
    fprintf(stderr, fmt, size_t(*p - begin) / total, *args...);
    fflush(stderr);
}
