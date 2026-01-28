#pragma once
#include "PCH.h"
#include "Image24.h"

namespace SDL_Client {

    class DrawArea {

    public:
        DrawArea() = default;
        ~DrawArea();
        DrawArea(int32_t width, int32_t height);
        DrawArea(const Image24& image);
        void Bind();
        void Draw(SDL_Surface* graphics, int32_t x, int32_t y);
    public:
        SDL_Surface* image = nullptr;
    private:
        int32_t width = 0;
        int32_t height = 0;

    };
}