#pragma once
#include "PCH.h"

namespace SDL_Client {

    class Signlink {
    public:
        static void Run();
        static int32_t GetUID(std::filesystem::path& dirPath);
        static void Unload();
        static void DNSLookup(const std::string& address);
    private:
        static std::filesystem::path FindCacheDir();
    public:
        static inline int32_t uid = -1;
        static inline std::filesystem::path cacheDatPath;
        static inline std::filesystem::path cacheIdxPath[5];
        static inline std::string dns;
        static inline std::filesystem::path soundFontPath;
    };
}