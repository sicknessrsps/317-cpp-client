#include "DrawArea.h"

#include "Draw2D.h"

namespace SDL_Client {
    DrawArea::~DrawArea() {
        SDL_DestroySurface(image);
    }

    DrawArea::DrawArea(int32_t width, int32_t height)
        : width(width), height(height)
    {
        image = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
        SDL_SetSurfaceBlendMode(image, SDL_BLENDMODE_NONE);
        Bind();
    }

    DrawArea::DrawArea(const Image24& image)
        : DrawArea(image.width, image.height)
    {
        image.BlitOpaque(0, 0);
    }

    void DrawArea::Bind()
    {
        Draw2D::Bind(image);
    }

    void DrawArea::Draw(SDL_Surface* graphics, int32_t x, int32_t y)
    {
        SDL_Rect dstRect{ x, y, 0, 0 };
        SDL_BlitSurface(image, nullptr, graphics, &dstRect); // back buffer?
    }
}
