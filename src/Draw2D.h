#pragma once
#include "PCH.h"

namespace SDL_Client {

    class Draw2D {
    public:
        ~Draw2D();
        static void Bind(SDL_Surface* surface);

        static void ResetBounds();
        static void SetBounds(int32_t l, int32_t t, int32_t r, int32_t b);

        static void Clear(uint32_t rgb = 0);

        static void DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t rgb);
        static void FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb, int32_t alpha);
        static void FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb);
        static void DrawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb);
        static void DrawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb, int32_t alpha);

        static void DrawLineX(int32_t x, int32_t y, int32_t length, uint32_t rgb);
        static void DrawLineX(int32_t x, int32_t y, int32_t length, uint32_t rgb, int32_t alpha);
        static void DrawLineY(int32_t x, int32_t y, int32_t length, uint32_t rgb);
        static void DrawLineY(uint32_t rgb, int32_t x, int32_t alpha, int32_t y, int32_t length);

    public:
        inline static SDL_Surface* surface = nullptr;
        inline static int32_t width = 0;
        inline static int32_t height = 0;
        inline static int32_t top = 0;
        inline static int32_t bottom = 0;
        inline static int32_t left = 0;
        inline static int32_t right = 0;
        inline static int32_t boundX = 0;
        inline static int32_t centerX = 0;
        inline static int32_t centerY = 0;

        static int32_t* Pixels() {
            return surface ? static_cast<int32_t*>(surface->pixels) : nullptr;
        }
    };

}
