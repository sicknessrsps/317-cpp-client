#pragma once
#include "PCH.h"
#include "Buffer.h"
#include "FileArchive.h"

namespace SDL_Client {
    class FloType {
    public:
        static void Unpack(FileArchive& archive);
        void Read(Buffer& in);
        void SetColor(int32_t newRgb);
    public:
        inline static int32_t count = 0;
        static std::vector<FloType> instances;
    private:
        static int32_t DecimateHSL(int32_t hue, int32_t saturation, int32_t lightness);
    public:
        // Public member variables
        int32_t rgb = 0;
        int32_t textureID = -1;
        bool occludes = true;
        int32_t hue = 0;
        int32_t saturation = 0;
        int32_t lightness = 0;
        int32_t chroma = 0;
        int32_t luminance = 0;
        int32_t hsl = 0;
    };

}