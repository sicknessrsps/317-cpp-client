#pragma once
#include "FileArchive.h"
#include "Buffer.h"

namespace SDL_Client
{
    class VarbitType
    {
    public:
        static void Unpack(FileArchive& archive);
    private:
        void Read(Buffer& in);
    public:
        static std::vector<std::shared_ptr<VarbitType>> instances;
        int32_t varp = 0;
        /**
         * The least significant bit.
         */
        int32_t lsb = 0;
        /**
         * The most significant bit.
         */
        int32_t msb = 0;
    };
}