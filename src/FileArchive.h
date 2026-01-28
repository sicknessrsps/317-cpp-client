#pragma once
#include "PCH.h"

namespace SDL_Client {

    class FileArchive {
    public:
        explicit FileArchive(const std::vector<int8_t>& src);
        std::vector<std::int8_t> Read(const std::string& src);
    private:
        void Load(const std::vector<int8_t>& s);
    private:
        std::vector<int8_t> data;
        bool m_Unpacked = false;
        int32_t m_FileCount{0};
        std::vector<int32_t> fileHash;
        std::vector<int32_t> fileSizeInflated;
        std::vector<int32_t> fileSizeDeflated;
        std::vector<int32_t> fileOffset;
    };

}