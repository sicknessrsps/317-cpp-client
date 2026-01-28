#include "Image24.h"

#include "Draw2D.h"
#include "Buffer.h"

namespace SDL_Client {

    Image24::~Image24() {
        if (surface) {
            SDL_DestroySurface(surface);
            surface = nullptr;
        }
    }

    Image24::Image24(const Image24& other)
        : width(other.width), height(other.height),
          cropX(other.cropX), cropY(other.cropY), cropW(other.cropW), cropH(other.cropH),
          pixels(other.pixels), window(other.window) {
        if (!pixels.empty()) {
            surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ARGB8888, pixels.data(), width * 4);
        }
    }

    Image24::Image24(Image24&& other) noexcept
        : surface(other.surface), width(other.width), height(other.height),
          cropX(other.cropX), cropY(other.cropY), cropW(other.cropW), cropH(other.cropH),
          pixels(std::move(other.pixels)), window(other.window) {
        other.surface = nullptr;
        other.width = other.height = other.cropX = other.cropY = other.cropW = other.cropH = 0;
        other.window = nullptr;
    }

    Image24& Image24::operator=(Image24&& other) noexcept {
        if (this != &other) {
            if (surface) SDL_DestroySurface(surface);
            surface = other.surface;
            width = other.width; height = other.height;
            cropX = other.cropX; cropY = other.cropY;
            cropW = other.cropW; cropH = other.cropH;
            pixels = std::move(other.pixels);
            window = other.window;
            other.surface = nullptr;
            other.width = other.height = other.cropX = other.cropY = other.cropW = other.cropH = 0;
            other.window = nullptr;
        }
        return *this;
    }

    int32_t Image24::Count(FileArchive& archive, const std::string& name) {
        auto datData = archive.Read(name + ".dat");
        auto idxData = archive.Read("index.dat");
        if (datData.empty() || idxData.empty()) return 0;

        Buffer dat(datData);
        Buffer idx(idxData);
        idx.position = dat.ReadU16();
        idx.ReadU16(); // cropW
        idx.ReadU16(); // cropH
        int32_t paletteSize = idx.ReadU8();
        idx.position += (paletteSize - 1) * 3;

        int32_t count = 0;
        int32_t datSize = static_cast<int32_t>(datData.size());
        while (dat.position < datSize && idx.position + 5 <= static_cast<int32_t>(idx.data.size())) {
            idx.position += 2;  // cropX, cropY
            int32_t w = idx.ReadU16();
            int32_t h = idx.ReadU16();
            if (w == 0 || h == 0) break;
            idx.position++;  // layout
            dat.position += w * h;
            count++;
        }
        return count;
    }

    Image24::Image24(const std::vector<int8_t>& src, SDL_Window* window)
    : window(window) {
        SDL_IOStream* img = SDL_IOFromConstMem(src.data(), src.size());
        if (!img) {
            LOG_ERROR("Couldn't load image from memory: %s", SDL_GetError());
        }

        // load image from memory (PNG, JPG, etc.)
        surface = IMG_Load_IO(img, true);
        if (!surface) {
            LOG_ERROR("Couldn't load image from memory: %s", SDL_GetError());
        }
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);

        width = surface->w;
        height = surface->h;
        cropW = width;
        cropH = height;
        cropX = cropY = 0;
    }

    Image24::Image24(FileArchive &archive, const std::string &file, int32_t index) {
        Buffer dat(archive.Read(file + ".dat"));
        Buffer idx(archive.Read("index.dat"));
        idx.position = dat.ReadU16();
        cropW = idx.ReadU16();
        cropH = idx.ReadU16();
        int32_t paletteSize = idx.ReadU8();

        std::vector<int32_t> palette(paletteSize);
        for (int32_t k = 0; k < paletteSize - 1; ++k) {
            int32_t rgb = idx.Read24();
            if (rgb == 0) {
                rgb = 1;
            }
            palette[k + 1] = 0xFF000000 | rgb;
        }

        for (int32_t i = 0; i < index; i++) {
            idx.position += 2;
            dat.position += idx.ReadU16() * idx.ReadU16();
            idx.position++;
        }

        cropX = idx.ReadU8();
        cropY = idx.ReadU8();
        width = idx.ReadU16();
        height = idx.ReadU16();
        int32_t layout = idx.ReadU8();

        int32_t pixelLen = width * height;
        pixels.resize(pixelLen);

        if (layout == 0) {
            for (int32_t i = 0; i < pixelLen; i++) {
                pixels[i] = palette[dat.ReadU8()];
            }
        } else if (layout == 1) {
            for (int32_t x = 0; x < width; x++) {
                for (int32_t y = 0; y < height; y++) {
                    pixels[x + y * width] = palette[dat.ReadU8()];
                }
            }
        }

        surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ARGB8888, pixels.data(), width*4);
        if (!surface) {
            LOG_ERROR("Couldn't load image from memory: %s", SDL_GetError());
        }
    }

    Image24::Image24(int32_t width, int32_t height)
        : width(width), height(height), cropW(width), cropH(height), cropX(0), cropY(0) {
        pixels.resize(width * height);
        surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ARGB8888, pixels.data(), width * 4);
    }

    void Image24::Save(const std::string& filename) {
        IMG_SavePNG(surface, filename.c_str());
        LOG_INFO("Saved png!");
    }

    void Image24::BlitOpaque(int32_t x, int32_t y) const {
        x += cropX;
        y += cropY;

        int32_t h = height;
        int32_t w = width;
        int32_t srcX = 0;
        int32_t srcY = 0;

        // Clip top
        if (y < Draw2D::top) {
            int32_t trim = Draw2D::top - y;
            h -= trim;
            srcY += trim;  // Skip trimmed rows in source
            y = Draw2D::top;
        }

        // Clip bottom
        if ((y + h) > Draw2D::bottom) {
            h -= (y + h) - Draw2D::bottom;
        }

        // Clip left
        if (x < Draw2D::left) {
            int32_t trim = Draw2D::left - x;
            w -= trim;
            srcX += trim;  // Skip trimmed columns in source
            x = Draw2D::left;
        }

        // Clip right
        if ((x + w) > Draw2D::right) {
            int32_t trim = (x + w) - Draw2D::right;
            w -= trim;
        }

        if ((w > 0) && (h > 0)) {
            SDL_Rect srcRect{ srcX, srcY, w, h };  // Source coordinates within the image
            SDL_Rect dstRect{ x, y, w, h };         // Destination coordinates on screen
            SDL_BlitSurface(surface, &srcRect, Draw2D::surface, &dstRect);
        }
    }

    void Image24::Bind() const
    {
        Draw2D::Bind(surface);
    }


    void Image24::Draw(int32_t x, int32_t y) {
        x += cropX;
        y += cropY;

        int32_t h = height;
        int32_t w = width;
        int32_t srcX = 0;
        int32_t srcY = 0;

        // Clip top
        if (y < Draw2D::top) {
            int32_t trim = Draw2D::top - y;
            h -= trim;
            srcY += trim;  // Skip trimmed rows in source
            y = Draw2D::top;
        }

        // Clip bottom
        if ((y + h) > Draw2D::bottom) {
            h -= (y + h) - Draw2D::bottom;
        }

        // Clip left
        if (x < Draw2D::left) {
            int32_t trim = Draw2D::left - x;
            w -= trim;
            srcX += trim;  // Skip trimmed columns in source
            x = Draw2D::left;
        }

        // Clip right
        if ((x + w) > Draw2D::right) {
            int32_t trim = (x + w) - Draw2D::right;
            w -= trim;
        }

        if ((w > 0) && (h > 0)) {
            // Manual pixel copy with transparency (skip pixels with value 0)
            int32_t* dstPixels = Draw2D::Pixels();
            if (!dstPixels || pixels.empty()) {
                return;
            }

            int32_t dstOff = x + (y * Draw2D::width);
            int32_t srcOff = srcX + (srcY * width);
            int32_t dstStep = Draw2D::width - w;
            int32_t srcStep = width - w;

            for (int32_t row = 0; row < h; row++) {
                for (int32_t col = 0; col < w; col++) {
                    int32_t pixel = pixels[srcOff++];
                    if (pixel != 0) {
                        dstPixels[dstOff] = pixel;
                    }
                    dstOff++;
                }
                dstOff += dstStep;
                srcOff += srcStep;
            }
        }
    }

    void Image24::Draw(int32_t x, int32_t y, int32_t alpha) {
        if (!surface || alpha <= 0) {
            return;
        }

        // Full opacity - use regular draw
        if (alpha >= 256) {
            Draw(x, y);
            return;
        }

        x += cropX;
        y += cropY;

        int32_t h = height;
        int32_t w = width;
        int32_t srcX = 0;
        int32_t srcY = 0;

        // Clip top
        if (y < Draw2D::top) {
            int32_t trim = Draw2D::top - y;
            h -= trim;
            srcY += trim;
            y = Draw2D::top;
        }

        // Clip bottom
        if ((y + h) > Draw2D::bottom) {
            h -= (y + h) - Draw2D::bottom;
        }

        // Clip left
        if (x < Draw2D::left) {
            int32_t trim = Draw2D::left - x;
            w -= trim;
            srcX += trim;
            x = Draw2D::left;
        }

        // Clip right
        if ((x + w) > Draw2D::right) {
            int32_t trim = (x + w) - Draw2D::right;
            w -= trim;
        }

        if ((w > 0) && (h > 0)) {
            // Convert alpha from 0-256 range to 0-255 range for SDL
            uint8_t sdlAlpha = static_cast<uint8_t>(std::min(alpha, 255));

            // Set alpha modulation and blend mode for this blit
            SDL_SetSurfaceAlphaMod(surface, sdlAlpha);
            SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

            SDL_Rect srcRect{ srcX, srcY, w, h };
            SDL_Rect dstRect{ x, y, w, h };
            SDL_BlitSurface(surface, &srcRect, Draw2D::surface, &dstRect);

            // Restore to full opacity and no blending for future draws
            SDL_SetSurfaceAlphaMod(surface, 255);
            SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
        }
    }

    void Image24::DrawRotatedMasked(int32_t x, int32_t y, int32_t w, int32_t h,
                                 int32_t anchorX, int32_t anchorY,
                                 int32_t zoom, int32_t angle,
                                 const std::vector<int32_t>& lineLengths,
                                 const std::vector<int32_t>& lineOffsets) {
        int32_t midX = -w / 2;
        int32_t midY = -h / 2;

        double angleRad = static_cast<double>(angle) / 326.11;
        int32_t sin = static_cast<int32_t>(std::sin(angleRad) * 65536.0);
        int32_t cos = static_cast<int32_t>(std::cos(angleRad) * 65536.0);

        sin = (sin * zoom) >> 8;
        cos = (cos * zoom) >> 8;

        int32_t leftX = (anchorX << 16) + (midY * sin) + (midX * cos);
        int32_t leftY = (anchorY << 16) + (midY * cos) - (midX * sin);

        int32_t leftOff = x + (y * Draw2D::width);
        int32_t* dstPixels = Draw2D::Pixels();

        if (! dstPixels || pixels.empty()) {
            return;
        }

        for (int32_t row = 0; row < h; row++) {
            int32_t lineOffset = lineOffsets[row];
            int32_t dstOff = leftOff + lineOffset;
            int32_t srcX = leftX + (cos * lineOffset);
            int32_t srcY = leftY - (sin * lineOffset);

            for (int32_t col = -lineLengths[row]; col < 0; col++) {
                int32_t srcIdx = (srcX >> 16) + ((srcY >> 16) * width);
                if (srcIdx >= 0 && srcIdx < static_cast<int32_t>(pixels.size()) &&
                    dstOff >= 0 && dstOff < Draw2D::width * Draw2D::height) {
                    // Both source (pixels) and destination (Draw2D) use ARGB8888
                    dstPixels[dstOff] = pixels[srcIdx];
                }
                dstOff++;
                srcX += cos;
                srcY -= sin;
            }

            leftX += sin;
            leftY += cos;
            leftOff += Draw2D::width;
        }
    }

    void Image24::DrawMasked(Image8& mask, int32_t y, int32_t x)
    {
        x += cropX;
        y += cropY;
        int32_t dstOff = x + (y * Draw2D::width);
        int32_t srcOff = 0;
        int32_t h = height;
        int32_t w = width;
        int32_t dstStep = Draw2D::width - w;
        int32_t srcStep = 0;
        if (y < Draw2D::top) {
            int32_t trim = Draw2D::top - y;
            h -= trim;
            y = Draw2D::top;
            srcOff += trim * w;
            dstOff += trim * Draw2D::width;
        }
        if ((y + h) > Draw2D::bottom) {
            h -= (y + h) - Draw2D::bottom;
        }
        if (x < Draw2D::left) {
            int32_t trim = Draw2D::left - x;
            w -= trim;
            x = Draw2D::left;
            srcOff += trim;
            dstOff += trim;
            srcStep += trim;
            dstStep += trim;
        }
        if ((x + w) > Draw2D::right) {
            int32_t trim = (x + w) - Draw2D::right;
            w -= trim;
            srcStep += trim;
            dstStep += trim;
        }
        if ((w > 0) && (h > 0)) {
            CopyPixelsMasked(pixels, srcOff, srcStep, mask.pixels, w, h, Draw2D::Pixels(), dstOff, dstStep);
        }
    }

    void Image24::Crop()
    {
        std::vector<int32_t> newPixels(cropW * cropH);
        for (int32_t y = 0; y < height; y++) {
            for (int32_t x = 0; x < width; x++) {
                newPixels[((y + cropY) * cropW) + x + cropX] = pixels[(y * width) + x];
            }
        }
        pixels = std::move(newPixels);
        width = cropW;
        height = cropH;
        cropX = 0;
        cropY = 0;
    }

    void Image24::DrawRotated(int32_t x, int32_t y, int32_t width, int32_t height, int32_t anchorX, int32_t anchorY,
                              float radians, int32_t zoom)
    {
        int32_t centerX = -width / 2;
        int32_t centerY = -height / 2;
        int32_t sinVal = static_cast<int32_t>(SDL_sinf(radians) * 65536.0f);
        int32_t cosVal = static_cast<int32_t>(SDL_cosf(radians) * 65536.0f);
        sinVal = (sinVal * zoom) >> 8;
        cosVal = (cosVal * zoom) >> 8;
        int32_t leftX = (anchorX << 16) + (centerY * sinVal) + (centerX * cosVal);
        int32_t leftY = ((anchorY << 16) + (centerY * cosVal)) - (centerX * sinVal);
        int32_t leftOff = x + (y * Draw2D::width);

        int32_t* dstPixels = Draw2D::Pixels();
        if (!dstPixels || pixels.empty()) {
            return;
        }

        for (int32_t row = 0; row < height; row++) {
            int32_t dstOff = leftOff;
            int32_t dstX = leftX;
            int32_t dstY = leftY;
            for (int32_t col = -width; col < 0; col++) {
                int32_t srcIdx = (dstX >> 16) + ((dstY >> 16) * this->width);
                if (srcIdx >= 0 && srcIdx < static_cast<int32_t>(pixels.size()) &&
                    dstOff >= 0 && dstOff < Draw2D::width * Draw2D::height) {
                    int32_t rgb = pixels[srcIdx];
                    if (rgb != 0) {
                        dstPixels[dstOff] = rgb;
                    }
                }
                dstOff++;
                dstX += cosVal;
                dstY -= sinVal;
            }
            leftX += sinVal;
            leftY += cosVal;
            leftOff += Draw2D::width;
        }
    }

    void Image24::Translate(int32_t r, int32_t g, int32_t b)
    {
        for (int32_t & pixel : pixels) {
            int32_t argb = pixel;
            if (argb != 0) {
                int32_t alpha = (argb >> 24) & 0xff;
                int32_t red = (argb >> 16) & 0xff;
                red += r;
                if (red < 1) {
                    red = 1;
                } else if (red > 255) {
                    red = 255;
                }
                int32_t green = (argb >> 8) & 0xff;
                green += g;
                if (green < 1) {
                    green = 1;
                } else if (green > 255) {
                    green = 255;
                }
                int32_t blue = argb & 0xff;
                blue += b;
                if (blue < 1) {
                    blue = 1;
                } else if (blue > 255) {
                    blue = 255;
                }
                pixel = (alpha << 24) | (red << 16) | (green << 8) | blue;
            }
        }
    }

    void Image24::CopyPixelsMasked(const std::vector<int32_t>& src, int32_t srcOff, int32_t srcStep,
                                   const std::vector<int8_t>& mask, int32_t w, int32_t h, int32_t* dst, int32_t dstOff, int32_t dstStep)
    {
        if (!dst) {
            return;
        }
        int32_t quarterW = -(w >> 2);
        w = -(w & 3);
        for (int32_t row = -h; row < 0; row++) {
            int32_t rgb;
            for (int32_t i = quarterW; i < 0; i++) {
                rgb = src[srcOff++];
                if ((rgb != 0) && (mask[dstOff] == 0)) {
                    dst[dstOff++] = rgb;
                } else {
                    dstOff++;
                }
                rgb = src[srcOff++];
                if ((rgb != 0) && (mask[dstOff] == 0)) {
                    dst[dstOff++] = rgb;
                } else {
                    dstOff++;
                }
                rgb = src[srcOff++];
                if ((rgb != 0) && (mask[dstOff] == 0)) {
                    dst[dstOff++] = rgb;
                } else {
                    dstOff++;
                }
                rgb = src[srcOff++];
                if ((rgb != 0) && (mask[dstOff] == 0)) {
                    dst[dstOff++] = rgb;
                } else {
                    dstOff++;
                }
            }
            for (int32_t i = w; i < 0; i++) {
                rgb = src[srcOff++];
                if ((rgb != 0) && (mask[dstOff] == 0)) {
                    dst[dstOff++] = rgb;
                } else {
                    dstOff++;
                }
            }
            dstOff += dstStep;
            srcOff += srcStep;
        }
    }

    void Image24::FlipHorizontal() const {
        if (!surface) {
            return;
        }

        // Lock the surface if necessary
        bool needsLock = SDL_MUSTLOCK(surface);
        if (needsLock) {
            SDL_LockSurface(surface);
        }

        // Get bytes per pixel from the surface format (SDL3 way)
        int32_t bytesPerPixel = SDL_BYTESPERPIXEL(surface->format);
        auto* pixels = static_cast<uint8_t*>(surface->pixels);
        int32_t pitch = surface->pitch;  // pitch in bytes

        // Temporary buffer for one pixel
        std::vector<uint8_t> tmpPixel(bytesPerPixel);

        // Flip each row
        for (int32_t y = 0; y < height; y++) {
            uint8_t* row = pixels + (y * pitch);

            // Swap pixels from both ends moving toward center
            for (int32_t x = 0; x < width / 2; x++) {
                int32_t leftIdx = x * bytesPerPixel;
                int32_t rightIdx = (width - x - 1) * bytesPerPixel;

                // Swap the pixels byte by byte
                for (int32_t b = 0; b < bytesPerPixel; b++) {
                    tmpPixel[b] = row[leftIdx + b];
                    row[leftIdx + b] = row[rightIdx + b];
                    row[rightIdx + b] = tmpPixel[b];
                }
            }
        }

        // Unlock the surface if we locked it
        if (needsLock) {
            SDL_UnlockSurface(surface);
        }
    }
}
