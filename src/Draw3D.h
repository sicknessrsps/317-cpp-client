#pragma once
#include "PCH.h"
#include "FileArchive.h"
#include "Image8.h"

namespace SDL_Client {

    class Draw3D {
    public:
        static void DrawScanline(int32_t* dst, int32_t offset, int32_t rgb, int32_t x0, int32_t x1);
        static void FillTriangle(int32_t y0, int32_t y1, int32_t y2, int32_t x0, int32_t x1, int32_t x2, int32_t color);

        static void FillGouraudTriangle(int32_t yA, int32_t yB, int32_t yC, int32_t xA,
            int32_t xB, int32_t xC, int32_t colorA, int32_t colorB, int32_t colorC);
        static void DrawGouraudScanline(int32_t* dst, int32_t offset, int32_t x0, int32_t x1, int32_t color0, int32_t color1);

        static void DrawTexturedScanline(int32_t* dst, std::vector<int32_t>& texels, int32_t curU, int32_t curV,
            int32_t offset, int32_t xA, int32_t xB, int32_t shadeA,
            int32_t shadeB, int32_t u, int32_t v, int32_t w, int32_t uStride, int32_t vStride, int32_t wStride);

        static void FillTexturedTriangle(int32_t yA, int32_t yB, int32_t yC, int32_t xA, int32_t xB, int32_t xC, int32_t shadeA,
            int32_t shadeB, int32_t shadeC, int32_t txA, int32_t txB, int32_t txC,
            int32_t tyA, int32_t tyB, int32_t tyC, int32_t tzA, int32_t tzB, int32_t tzC, int32_t texture);

        static void Init3D(int32_t width, int32_t height);
        static void Init2D();
        static void SetBrightness(double brightness);
        static void InitPool(int32_t poolSize);
        static void ClearTexels();

        static void UnpackTextures(FileArchive& archive);

        static std::vector<int32_t>& GetTexels(int32_t textureID);
        static int32_t GetAverageTextureRGB(int32_t textureID);
        static int32_t SetGamma(int32_t rgb, double gamma);
        static void PushTexture(int32_t textureID);

        static void Unload();
    public:
        /**
         * Setting this to <code>true</code> enables horizontal clipping for scanlines.
         */
        inline static bool clipX = false;
        inline static int32_t poolSize = 0;
        inline static int32_t alpha = 0;
        inline static std::vector<int32_t> lineOffset;
        inline static int32_t centerX = 0;
        inline static int32_t centerY = 0;
        inline static int32_t textureCount = 0;

        inline static bool lowmem = false;
        inline static std::array<int32_t, 50> textureCycle{};
        inline static int32_t cycle = 0;

        inline static std::array<Image8, 50> textures{};
        inline static std::array<std::vector<int32_t>, 50> texturePalette{};
        inline static std::array<int32_t, 50> averageTextureRGB{};

        inline static std::array<std::vector<int32_t>, 50> activeTexels{};
        inline static std::vector<std::vector<int32_t>> texelPool;
        inline static std::array<int8_t, 50> textureTranslucent{};

        /**
         * Used with {@link #fillTexturedTriangle(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int)} to
         * avoid branching.
         */
        inline static bool opaque = false;

        /**
         * Unrolls the loops for drawing gouraud triangles to produce a jagged look.
         *
         * @see #drawGouraudScanline(int[], int, int, int, int, int)
         */
        inline static bool jagged = true;

        static constexpr int32_t SIN_COS_SIZE = 2048;
        static constexpr int32_t PALETTE_SIZE = 0x10000;
        static constexpr int32_t RECIP16_SIZE = 2048;
        static constexpr int32_t RECIP15_SIZE = 512;

        inline static const std::array<int32_t, RECIP15_SIZE> reciprocal15 = [] {
            std::array<int32_t, RECIP15_SIZE> arr{};
            for (int32_t i = 1; i < RECIP15_SIZE; i++)
                arr[i] = (1 << 15) / i;
            return arr;
        }();

        inline static const std::array<int32_t, RECIP16_SIZE> reciprocal16 = [] {
            std::array<int32_t, RECIP16_SIZE> arr{};
            for (int32_t i = 1; i < RECIP16_SIZE; i++)
                arr[i] = (1 << 16) / i;
            return arr;
        }();

        inline static const std::array<int32_t, SIN_COS_SIZE> sin = [] {
            std::array<int32_t, SIN_COS_SIZE> arr{};
            for (int32_t k = 0; k < SIN_COS_SIZE; k++) {
                double angle = k * 0.0030679614999999999; // 2π/2048
                arr[k] = static_cast<int32_t>(65536.0 * std::sin(angle));
            }
            return arr;
        }();

        inline static const std::array<int32_t, SIN_COS_SIZE> cos = [] {
            std::array<int32_t, SIN_COS_SIZE> arr{};
            for (int32_t k = 0; k < SIN_COS_SIZE; k++) {
                double angle = k * 0.0030679614999999999;
                arr[k] = static_cast<int32_t>(65536.0 * std::cos(angle));
            }
            return arr;
        }();

        inline static std::array<int32_t, 0x10000> palette{};

        inline static int32_t UnsignedShift(int32_t value, int32_t bits) {
            return static_cast<int32_t>(static_cast<uint32_t>(value) >> bits);
        }

    };
}