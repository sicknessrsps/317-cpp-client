#include "Draw2D.h"

namespace SDL_Client {

    Draw2D::~Draw2D() {
        // Don't destroy surface - Draw2D doesn't own it, just holds a reference
        // The surface is owned and destroyed by whoever created it (e.g., GameShell::drawSurface)
        surface = nullptr;
    }

    void Draw2D::Bind(SDL_Surface* surf) {
        surface = surf;
        width = surface->w;
        height = surface->h;
        SetBounds(0, 0, width, height);
    }

    void Draw2D::ResetBounds() {
        left = 0;
        top = 0;
        right = width;
        bottom = height;
        boundX = right - 1;
        centerX = right / 2;
        centerY = bottom / 2;
    }

    void Draw2D::SetBounds(int32_t l, int32_t t, int32_t r, int32_t b) {
        left = std::max(0, l);
        top = std::max(0, t);
        right = std::min(width, r);
        bottom = std::min(height, b);
        boundX = right - 1;
        centerX = right / 2;
        centerY = bottom / 2;
    }

    void Draw2D::Clear(uint32_t rgb) {
        if (!surface) return;
        std::fill_n(Pixels(), width * height, rgb);
    }

    void Draw2D::DrawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t rgb) {
        int32_t dx = std::abs(x2 - x1);
        int32_t dy = std::abs(y2 - y1);
        int32_t sx = (x1 < x2) ? 1 : -1;
        int32_t sy = (y1 < y2) ? 1 : -1;
        int32_t err = dx - dy;

        while (true) {
            if ((x1 >= left) && (x1 < right) && (y1 >= top) && (y1 < bottom)) {
                Pixels()[x1 + y1 * width] =  0xFF000000 | rgb;
            }
            if (x1 == x2 && y1 == y2) break;
            int32_t e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x1 += sx; }
            if (e2 < dx) { err += dx; y1 += sy; }
        }
    }

    void Draw2D::FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb, int32_t alpha) {
        if (alpha <= 0) return;
        if (alpha >= 256) { FillRect(x, y, w, h, rgb); return; }

        if (x < left) { w -= left - x; x = left; }
        if (y < top) { h -= top - y; y = top; }
        if ((x + w) > right) w = right - x;
        if ((y + h) > bottom) h = bottom - y;

        int32_t invAlpha = 256 - alpha;
        int32_t r0 = ((rgb >> 16) & 0xff) * alpha;
        int32_t g0 = ((rgb >> 8) & 0xff) * alpha;
        int32_t b0 = (rgb & 0xff) * alpha;

        int32_t step = width - w;
        int32_t offset = x + y * width;
        auto* px = Pixels();

        for (int32_t i = 0; i < h; i++) {
            for (int32_t j = 0; j < w; j++) {
                int32_t r1 = ((px[offset] >> 16) & 0xff) * invAlpha;
                int32_t g1 = ((px[offset] >> 8) & 0xff) * invAlpha;
                int32_t b1 = (px[offset] & 0xff) * invAlpha;
                px[offset++] =  0xFF000000 | (((r0 + r1) >> 8) << 16) |
                               (((g0 + g1) >> 8) << 8) |
                               ((b0 + b1) >> 8);
            }
            offset += step;
        }
    }

    void Draw2D::FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb) {
        if (x < left) { w -= left - x; x = left; }
        if (y < top) { h -= top - y; y = top; }
        if ((x + w) > right) w = right - x;
        if ((y + h) > bottom) h = bottom - y;

        int32_t step = width - w;
        int32_t offset = x + y * width;
        auto* px = Pixels();

        for (int32_t i = 0; i < h; i++) {
            for (int32_t j = 0; j < w; j++) {
                px[offset++] =  0xFF000000 | rgb;
            }
            offset += step;
        }
    }

    void Draw2D::DrawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb) {
        DrawLineX(x, y, w, rgb);
        DrawLineX(x, y + h - 1, w, rgb);
        DrawLineY(x, y, h, rgb);
        DrawLineY(x + w - 1, y, h, rgb);
    }

    void Draw2D::DrawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb, int32_t alpha) {
        DrawLineX(x, y, w, rgb, alpha);
        DrawLineX(x, y + h - 1, w, rgb, alpha);
        if (h >= 3) {
            DrawLineY(rgb, x, alpha, y + 1, h - 2);
            DrawLineY(rgb, x + w - 1, alpha, y + 1, h - 2);
        }
    }

    void Draw2D::DrawLineX(int32_t x, int32_t y, int32_t length, uint32_t rgb) {
        if (y < top || y >= bottom) return;
        if (x < left) { length -= left - x; x = left; }
        if ((x + length) > right) length = right - x;
        auto* px = Pixels();
        int32_t offset = x + y * width;
        for (int32_t i = 0; i < length; i++) px[offset + i] =  0xFF000000 |rgb;
    }

    void Draw2D::DrawLineX(int32_t x, int32_t y, int32_t length, uint32_t rgb, int32_t alpha) {
        if (y < top || y >= bottom) return;
        if (alpha <= 0) return;
        if (alpha >= 256) { DrawLineX(x, y, length, rgb); return; }
        if (x < left) { length -= left - x; x = left; }
        if ((x + length) > right) length = right - x;

        int32_t invAlpha = 256 - alpha;
        int32_t r0 = ((rgb >> 16) & 0xff) * alpha;
        int32_t g0 = ((rgb >> 8) & 0xff) * alpha;
        int32_t b0 = (rgb & 0xff) * alpha;

        auto* px = Pixels();
        int32_t offset = x + y * width;

        for (int32_t i = 0; i < length; i++) {
            int32_t r1 = ((px[offset] >> 16) & 0xff) * invAlpha;
            int32_t g1 = ((px[offset] >> 8) & 0xff) * invAlpha;
            int32_t b1 = (px[offset] & 0xff) * invAlpha;
            px[offset++] =  0xFF000000 | (((r0 + r1) >> 8) << 16) |
                           (((g0 + g1) >> 8) << 8) |
                           ((b0 + b1) >> 8);
        }
    }

    void Draw2D::DrawLineY(int32_t x, int32_t y, int32_t length, uint32_t rgb) {
        if (x < left || x >= right) return;
        if (y < top) { length -= top - y; y = top; }
        if ((y + length) > bottom) length = bottom - y;
        auto* px = Pixels();
        int32_t offset = x + y * width;
        for (int32_t i = 0; i < length; i++) {
            px[offset + i * width] =  0xFF000000 |rgb;
        }
    }

    void Draw2D::DrawLineY(uint32_t rgb, int32_t x, int32_t alpha, int32_t y, int32_t length) {
        if (x < left || x >= right) return;
        if (alpha <= 0) return;
        if (alpha >= 256) { DrawLineY(x, y, length, rgb); return; }
        if (y < top) { length -= top - y; y = top; }
        if ((y + length) > bottom) length = bottom - y;

        int32_t invAlpha = 256 - alpha;
        int32_t r0 = ((rgb >> 16) & 0xff) * alpha;
        int32_t g0 = ((rgb >> 8) & 0xff) * alpha;
        int32_t b0 = (rgb & 0xff) * alpha;

        auto* px = Pixels();
        int32_t offset = x + y * width;
        for (int32_t i = 0; i < length; i++) {
            int32_t r1 = ((px[offset] >> 16) & 0xff) * invAlpha;
            int32_t g1 = ((px[offset] >> 8) & 0xff) * invAlpha;
            int32_t b1 = (px[offset] & 0xff) * invAlpha;
            px[offset] =  0xFF000000 | (((r0 + r1) >> 8) << 16) |
                         (((g0 + g1) >> 8) << 8) |
                         ((b0 + b1) >> 8);
            offset += width;
        }
    }

}
