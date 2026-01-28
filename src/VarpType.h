#pragma once
#include "FileArchive.h"
#include "Buffer.h"

namespace SDL_Client
{
    class VarpType
    {
    public:
        static void Unpack(FileArchive& archive);
    private:
        void Read(Buffer& in);
    public:
        static std::vector<std::shared_ptr<VarpType>> instances;
        int32_t type = 0;
    };
}