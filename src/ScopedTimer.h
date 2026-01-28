#pragma once
#include "PCH.h"

class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& label)
        : name(label), start(SDL_GetPerformanceCounter()) {}

    ~ScopedTimer() {
        Uint64 end = SDL_GetPerformanceCounter();
        double ms = (double)(end - start) * 1000.0 / SDL_GetPerformanceFrequency();
        LOG_INFO("%s took %f ms", name.c_str(), ms);
    }

private:
    std::string name;
    Uint64 start;
};