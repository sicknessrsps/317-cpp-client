#pragma once
#include "PCH.h"

namespace SDL_Client {

    class FileStore {
    public:
        FileStore(int32_t maxFileSize, const std::filesystem::path& datPath, const std::filesystem::path& idxPath, int32_t store);

        int64_t GetFileCount();
        std::vector<int8_t> Read(int32_t file);
        void Write(const std::vector<int8_t>& src, int32_t file, int32_t size);
        bool Write(const std::vector<int8_t>& data, int32_t file, int32_t size, bool overwrite);

        void Unload();
    public:
        static constexpr int32_t BUF_SIZE = 520;
        static int8_t buf[BUF_SIZE];
    private:
        std::filesystem::path datPath;
        std::filesystem::path idxPath;
        int32_t store = 0;
        int32_t maxFileSize = 0;
        std::mutex file_mutex;

    };

}