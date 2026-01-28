#pragma once
#include "PCH.h"
#include "FileArchive.h"

namespace SDL_Client {
    class Image8 {
    public:
        Image8() = default;
        ~Image8();
        Image8(const Image8&) = delete;
        Image8& operator=(const Image8&) = delete;
        Image8(Image8&& other) noexcept;
        Image8& operator=(Image8&& other) noexcept;
        Image8(FileArchive& archive, const std::string& name, int32_t index);
        static int32_t Count(FileArchive& archive, const std::string& name);
        void Save(const std::string& filename);
        void Translate(int32_t r, int32_t g, int32_t b);
        void Blit(int32_t x, int32_t y);
        void FlipHorizontally();
        void FlipVertically();
        void Crop();
        void Shrink();
    private:
        void RecreateSurface();
    public:
        int32_t width = 0;
        int32_t height = 0;
        int32_t cropX = 0;
        int32_t cropY = 0;
        SDL_Surface* surface = nullptr;
        std::vector<int32_t> palette;
        std::vector<int8_t> pixels;
        int32_t cropW = 0;
        int32_t cropH = 0;
    };
}