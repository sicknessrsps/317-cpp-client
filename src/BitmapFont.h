#pragma once
#include "PCH.h"
#include "FileArchive.h"

namespace SDL_Client {
    class BitmapFont {
    public:
        BitmapFont(FileArchive& archive, const std::string& name, bool quill);

        void DrawStringRight(const std::string& s, int32_t x, int32_t y, int32_t rgb);
        void DrawStringCenter(const std::string& s, int32_t x, int32_t y, uint32_t rgb);
        void DrawStringTaggableCenter(const std::string& s, int32_t x, int32_t y, uint32_t rgb, bool shadow);

        int32_t StringWidthTaggable(const std::string& s) const;
        int32_t StringWidth(const std::string& s) const;

        void DrawString(const std::string& s, int32_t x, int32_t y, uint32_t rgb);
        void DrawStringWave(const std::string& s, int32_t x, int32_t y, int32_t rgb, int32_t cycle);
        void DrawStringWave2(const std::string& s, int32_t x, int32_t y, int32_t rgb, int32_t cycle);
        void DrawStringShake(const std::string& s, int32_t x, int32_t y, int32_t rgb, int32_t cycle, int32_t phase);

        void DrawStringTaggable(const std::string& s, int32_t x, int32_t y, uint32_t rgb, bool shadowed);
        void DrawStringTooltip(const std::string& s, int32_t x, int32_t y, int32_t rgb, bool shadowed, int32_t seed);
    public:
        int32_t height = 0;
    private:
        uint32_t EvaluateTag(const std::string& s);

        void FillMaskedRect(const std::vector<int8_t>& mask, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb);
        /*void FillMaskedRect(const std::vector<int8_t>& mask, int32_t x, int32_t y, int32_t w, int32_t h, int32_t rgb, int32_t alpha);*/
        void FillMaskedRect(const std::vector<int8_t>& mask, int32_t maskOff, int32_t maskStep, int32_t* dst,
            int32_t dstOff, int32_t dstStep, int32_t w, int32_t h, uint32_t rgb);
        /*void FillMaskedRect(const std::vector<int8_t>& mask, int32_t maskOff, int32_t maskStep,
                            std::vector<int32_t>& dst, int32_t dstOff, int32_t dstStep,
                            int32_t w, int32_t h, int32_t rgb, int32_t alpha);*/

        std::vector<std::vector<int8_t>> charMask;
        std::array<int32_t, 256> charMaskWidth{};
        std::array<int32_t, 256> charMaskHeight{};
        std::array<int32_t, 256> charOffsetX{};
        std::array<int32_t, 256> charOffsetY{};
        std::array<int32_t, 256> charAdvance{};

        std::mt19937 random;
        bool strikethrough{false};
    };
}