#pragma once
#include "PCH.h"
#include "FileArchive.h"
#include "Image8.h"
#include "DoublyLinkedList.h"

namespace SDL_Client {
    class Image24 : public DoublyLinkedList::Node {
    public:
        Image24() = default;
        ~Image24();
        Image24(const Image24& other);
        Image24& operator=(const Image24&) = delete;
        Image24(Image24&& other) noexcept;
        Image24& operator=(Image24&& other) noexcept;
        static int32_t Count(FileArchive& archive, const std::string& name);
        explicit Image24(const std::vector<int8_t>& src, SDL_Window* window);
        Image24(FileArchive& archive, const std::string& file, int32_t index);
        Image24(int32_t width, int32_t height);
        void Save(const std::string& filename);
        void BlitOpaque(int32_t x, int32_t y) const;
        void Bind() const;
        void Draw(int32_t x, int32_t y);
        void Draw(int32_t x, int32_t y, int32_t alpha);
        void DrawRotatedMasked(int32_t x, int32_t y, int32_t w, int32_t h,
                                 int32_t anchorX, int32_t anchorY,
                                 int32_t zoom, int32_t angle,
                                 const std::vector<int32_t>& lineLengths,
                                 const std::vector<int32_t>& lineOffsets);
        void DrawMasked(Image8& mask, int32_t y, int32_t x);
        void Crop();
        void DrawRotated(int32_t x, int32_t y, int32_t width, int32_t height,
            int32_t anchorX, int32_t anchorY, float radians, int32_t zoom);
        void Translate(int32_t r, int32_t g, int32_t b);
        static void CopyPixelsMasked(const std::vector<int32_t>& src, int32_t srcOff, int32_t srcStep,
            const std::vector<int8_t>& mask, int32_t w, int32_t h, int32_t* dst, int32_t dstOff, int32_t dstStep);
        void FlipHorizontal() const;
    public:
        SDL_Surface* surface = nullptr;
        int32_t width = 0;
        int32_t height = 0;
        int32_t cropX = 0;
        int32_t cropY = 0;
        int32_t cropW = 0;
        int32_t cropH = 0;
        std::vector<int32_t> pixels;
    private:
        SDL_Window* window = nullptr;
    };
}