#include "Image8.h"
#include "Buffer.h"
#include "Draw2D.h"

namespace SDL_Client {

    Image8::~Image8() {
        if (surface) {
            SDL_DestroySurface(surface);
            surface = nullptr;
        }
    }

    Image8::Image8(Image8&& other) noexcept
        : width(other.width), height(other.height), cropX(other.cropX), cropY(other.cropY),
          surface(other.surface), palette(std::move(other.palette)), pixels(std::move(other.pixels)),
          cropW(other.cropW), cropH(other.cropH) {
        other.surface = nullptr;
        other.width = other.height = other.cropX = other.cropY = other.cropW = other.cropH = 0;
    }

    Image8& Image8::operator=(Image8&& other) noexcept {
        if (this != &other) {
            if (surface) SDL_DestroySurface(surface);
            width = other.width; height = other.height;
            cropX = other.cropX; cropY = other.cropY;
            cropW = other.cropW; cropH = other.cropH;
            surface = other.surface;
            palette = std::move(other.palette);
            pixels = std::move(other.pixels);
            other.surface = nullptr;
            other.width = other.height = other.cropX = other.cropY = other.cropW = other.cropH = 0;
        }
        return *this;
    }

    int32_t Image8::Count(FileArchive& archive, const std::string &name) {
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
            idx.position++;  // pixelOrder
            dat.position += w * h;
            count++;
        }
        return count;
    }

    Image8::Image8(FileArchive& archive, const std::string &name, int32_t index) {
        Buffer dat(archive.Read(name + ".dat"));
        Buffer idx(archive.Read("index.dat"));

        idx.position = dat.ReadU16();
        cropW = idx.ReadU16();
        cropH = idx.ReadU16();
        int32_t paletteSize = idx.ReadU8();
        palette.resize(paletteSize);
        for (int32_t i = 0; i < (paletteSize - 1); i++) {
            palette[i + 1] = idx.Read24();
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
        int32_t pixelOrder = idx.ReadU8();
        int32_t pixelCount = width * height;

        pixels.resize(pixelCount);
        if (pixelOrder == 0) {
            for (int32_t i = 0; i < pixelCount; i++) {
                pixels[i] = dat.Read8();
            }
        } else if (pixelOrder == 1) {
            for (int32_t x = 0; x < width; x++) {
                for (int32_t y = 0; y < height; y++) {
                    pixels[x + (y * width)] = dat.Read8();
                }
            }
        }

        surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_INDEX8, pixels.data(), width);
        if (!surface) {
            LOG_ERROR("Couldn't load image from memory: %s", SDL_GetError());
            return;
        }

        // Set up palette
        SDL_Palette* sdl_palette = SDL_CreatePalette(paletteSize);
        for (int32_t i = 0; i < paletteSize; i++) {
            uint32_t rgb = palette[i];
            sdl_palette->colors[i].r = (rgb >> 16) & 0xFF;
            sdl_palette->colors[i].g = (rgb >> 8) & 0xFF;
            sdl_palette->colors[i].b = rgb & 0xFF;
            sdl_palette->colors[i].a = 255;
        }
        SDL_SetSurfacePalette(surface, sdl_palette);

        // Set color key for transparency (index 0 = transparent)
        SDL_SetSurfaceColorKey(surface, true, 0);
    }



    void Image8::Save(const std::string &filename) {
        IMG_SavePNG(surface, filename.c_str());
        LOG_INFO("Saved png!");
    }

    void Image8::Translate(int32_t r, int32_t g, int32_t b)
    {
        for (int32_t & i : palette) {
            int32_t red = (i >> 16) & 0xff;
            red += r;
            if (red < 0) {
                red = 0;
            } else if (red > 255) {
                red = 255;
            }
            int32_t green = (i >> 8) & 0xff;
            green += g;
            if (green < 0) {
                green = 0;
            } else if (green > 255) {
                green = 255;
            }
            int32_t blue = i & 0xff;
            blue += b;
            if (blue < 0) {
                blue = 0;
            } else if (blue > 255) {
                blue = 255;
            }
            i = (red << 16) + (green << 8) + blue;
        }

        // Update SDL palette to match
        SDL_Palette* sdl_palette = SDL_GetSurfacePalette(surface);
        if (sdl_palette) {
            for (size_t i = 0; i < palette.size(); i++) {
                uint32_t rgb = palette[i];
                sdl_palette->colors[i].r = (rgb >> 16) & 0xFF;
                sdl_palette->colors[i].g = (rgb >> 8) & 0xFF;
                sdl_palette->colors[i].b = rgb & 0xFF;
                sdl_palette->colors[i].a = 255;
            }
        }
    }

    void Image8::Blit(int32_t x, int32_t y) {
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

    void Image8::FlipHorizontally()
    {
        std::vector<int8_t> flippedPixels(width * height);
        int32_t off = 0;
        for (int32_t y = 0; y < height; y++) {
            for (int32_t x = width - 1; x >= 0; x--) {
                flippedPixels[off++] = pixels[x + (y * width)];
            }
        }
        pixels = std::move(flippedPixels);
        cropX = cropW - width - cropX;
        RecreateSurface();
    }

    void Image8::FlipVertically()
    {
        std::vector<int8_t> flippedPixels(width * height);
        int32_t i = 0;
        for (int32_t y = height - 1; y >= 0; y--) {
            for (int32_t x = 0; x < width; x++) {
                flippedPixels[i++] = pixels[x + (y * width)];
            }
        }
        pixels = std::move(flippedPixels);
        cropY = cropH - height - cropY;
        RecreateSurface();
    }

    void Image8::Shrink() {
        cropW /= 2;
        cropH /= 2;
        std::vector<int8_t> newPixels(cropW * cropH);
        int32_t off = 0;
        for (int32_t y = 0; y < height; y++) {
            for (int32_t x = 0; x < width; x++) {
                newPixels[((x + cropX) >> 1) + (((y + cropY) >> 1) * cropW)] = pixels[off++];
            }
        }
        pixels = std::move(newPixels);
        width = cropW;
        height = cropH;
        cropX = 0;
        cropY = 0;
        RecreateSurface();
    }

    void Image8::Crop() {
        if ((width == cropW) && (height == cropH)) {
            return;
        }
        std::vector<int8_t> pix(cropW * cropH);
        int32_t off = 0;
        for (int32_t y = 0; y < height; y++) {
            for (int32_t x = 0; x < width; x++) {
                pix[x + cropX + ((y + cropY) * cropW)] = pixels[off++];
            }
        }
        pixels = std::move(pix);
        width = cropW;
        height = cropH;
        cropX = 0;
        cropY = 0;
        RecreateSurface();
    }

    void Image8::RecreateSurface() {
        // Destroy old surface
        if (surface) {
            SDL_DestroySurface(surface);
        }

        // Create new surface with new pixel data
        surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_INDEX8, pixels.data(), width);
        if (!surface) {
            LOG_ERROR("Couldn't recreate surface: %s", SDL_GetError());
            return;
        }

        // Recreate palette from internal palette vector
        SDL_Palette* newPalette = SDL_CreatePalette(static_cast<int32_t>(palette.size()));
        for (size_t i = 0; i < palette.size(); i++) {
            uint32_t rgb = palette[i];
            newPalette->colors[i].r = (rgb >> 16) & 0xFF;
            newPalette->colors[i].g = (rgb >> 8) & 0xFF;
            newPalette->colors[i].b = rgb & 0xFF;
            newPalette->colors[i].a = 255;
        }
        SDL_SetSurfacePalette(surface, newPalette);
        SDL_SetSurfaceColorKey(surface, true, 0);
    }
}
