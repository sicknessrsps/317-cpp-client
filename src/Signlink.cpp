#include "Signlink.h"

namespace SDL_Client {

    void Signlink::Run()
    {
        std::filesystem::path cacheDir = FindCacheDir();
        if (!std::filesystem::exists(cacheDir)) {
            std::string absPath = std::filesystem::absolute(cacheDir).string();
            LOG_ERROR("Cache directory not found: %s", absPath.c_str());
        }

        // Store paths - files will be opened on-demand by FileStore
        cacheDatPath = cacheDir / "main_file_cache.dat";

        // Create dat file if it doesn't exist
        if (!std::filesystem::exists(cacheDatPath)) {
            SDL_IOStream* out = SDL_IOFromFile(cacheDatPath.string().c_str(), "wb");
            if (out) {
                SDL_CloseIO(out);
            } else {
                LOG_ERROR("Failed to create cache dat file: %s", SDL_GetError());
            }
        }

        for (int32_t j = 0; j < 5; j++) {
            cacheIdxPath[j] = cacheDir / ("main_file_cache.idx" + std::to_string(j));

            // Create idx file if it doesn't exist
            if (!std::filesystem::exists(cacheIdxPath[j])) {
                SDL_IOStream* out = SDL_IOFromFile(cacheIdxPath[j].string().c_str(), "wb");
                if (out) {
                    SDL_CloseIO(out);
                } else {
                    LOG_ERROR("Failed to create cache idx file %d: %s", j, SDL_GetError());
                }
            }
        }
        soundFontPath = cacheDir / "Old_School_RuneScape.sf2";
        uid = GetUID(cacheDir);
    }

    int32_t Signlink::GetUID(std::filesystem::path& dirPath)
    {
        std::filesystem::path uidPath = dirPath / "uid.dat";
        std::string uidPathStr = uidPath.string();

        // Check if file exists and has valid size
        if (!std::filesystem::exists(uidPath) || std::filesystem::file_size(uidPath) < 4) {
            // Create new UID file
            SDL_IOStream* out = SDL_IOFromFile(uidPathStr.c_str(), "wb");
            if (!out) {
                LOG_ERROR("Failed to create uid.dat: %s", SDL_GetError());
                return 0;
            }

            // Generate random UID (0 to 99999999)
            auto newUID = SDL_rand(100000000);

            // Write as big-endian (Java's writeInt format)
            uint8_t bytes[4];
            bytes[0] = static_cast<uint8_t>((newUID >> 24) & 0xFF);
            bytes[1] = static_cast<uint8_t>((newUID >> 16) & 0xFF);
            bytes[2] = static_cast<uint8_t>((newUID >> 8) & 0xFF);
            bytes[3] = static_cast<uint8_t>(newUID & 0xFF);

            if (SDL_WriteIO(out, bytes, 4) != 4) {
                LOG_ERROR( "Failed to write uid.dat: %s", SDL_GetError());
                SDL_CloseIO(out);
                return 0;
            }

            SDL_CloseIO(out);
        }

        SDL_IOStream* in = SDL_IOFromFile(uidPathStr.c_str(), "rb");
        if (!in) {
            LOG_ERROR("Failed to open uid.dat: %s", SDL_GetError());
            return 0;
        }

        uint8_t bytes[4];
        if (SDL_ReadIO(in, bytes, 4) != 4) {
            LOG_ERROR( "Failed to read uid.dat: %s", SDL_GetError());
            SDL_CloseIO(in);
            return 0;
        }

        SDL_CloseIO(in);

        // Read as big-endian (Java's readInt format)
        int32_t uid = ((bytes[0] & 0xFF) << 24) |
                      ((bytes[1] & 0xFF) << 16) |
                      ((bytes[2] & 0xFF) << 8) |
                      (bytes[3] & 0xFF);

        return uid + 1;
    }

    void Signlink::Unload()
    {
        // Paths don't need cleanup - files are opened/closed on-demand
        cacheDatPath.clear();
        for (auto& path : cacheIdxPath) {
            path.clear();
        }
        soundFontPath.clear();
        dns.clear();
    }

    void Signlink::DNSLookup(const std::string& address)
    {
        // Simple approach: store IP address directly
        // (SDL3_net doesn't support reverse DNS lookup)
        dns = address;
    }

    std::filesystem::path Signlink::FindCacheDir()
    {
        // SDL3: SDL_GetBasePath() returns internally cached memory - do NOT free it
        const char* basePathPtr = SDL_GetBasePath();
        if (!basePathPtr) {
            LOG_ERROR("Base path not found: %s", SDL_GetError());
            return {};
        }
        const std::filesystem::path basePath = basePathPtr;

        const auto cachePath = basePath / "cache";
        return cachePath;
    }
}
