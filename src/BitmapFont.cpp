#include "BitmapFont.h"
#include "Buffer.h"
#include "Draw2D.h"

namespace SDL_Client {

    BitmapFont::BitmapFont(FileArchive &archive, const std::string &name, bool quill)
        : charMask(256)
    {
        Buffer dat(archive.Read(name + ".dat"));
        Buffer idx(archive.Read("index.dat"));
        idx.position = dat.ReadU16() + 4;

        int32_t k = idx.ReadU8();

        if (k > 0) {
            idx.position += 3 * (k - 1);
        }

        for (int32_t c = 0; c < 256; c++) {
            charOffsetX[c] = idx.ReadU8();
            charOffsetY[c] = idx.ReadU8();

            int32_t w = charMaskWidth[c] = idx.ReadU16();
            int32_t h = charMaskHeight[c] = idx.ReadU16();
            int32_t storeOrder = idx.ReadU8();

            int32_t len = w * h;
            charMask[c] = std::vector<int8_t>(len);

            if (storeOrder == 0) {
                for (int32_t i = 0; i < len; i++) {
                    charMask[c][i] = dat.Read8();
                }
            } else if (storeOrder == 1) {
                for (int32_t x = 0; x < w; x++) {
                    for (int32_t y = 0; y < h; y++) {
                        charMask[c][x + (y * w)] = dat.Read8();
                    }
                }
            }

            if ((h > height) && (c < 128)) {
                height = h;
            }

            // some simple kerning
            // https://en.wikipedia.org/wiki/Kerning

            charOffsetX[c] = 1;
            charAdvance[c] = w + 2;

            int32_t acc = 0;

            for (int32_t y = h / 7; y < h; y++) {
                acc += charMask[c][y * w];
            }

            if (acc <= (h / 7)) {
                charAdvance[c]--;
                charOffsetX[c] = 0;
            }

            acc = 0;

            for (int32_t y = h / 7; y < h; y++) {
                acc += charMask[c][(w - 1) + (y * w)];
            }

            if (acc <= (h / 7)) {
                charAdvance[c]--;
            }
        }

        // only q8_full uses this flag.
        if (quill) {
            charAdvance[' '] = charAdvance['I'];
        } else {
            charAdvance[' '] = charAdvance['i'];
        }

    }

    void BitmapFont::DrawStringCenter(const std::string &s, const int32_t x, const int32_t y, const uint32_t rgb)
    {
        DrawString(s, x - (StringWidth(s) / 2), y, rgb);
    }

    /**
     * Draws a centered and taggable string.
     *
     * @param s      the string.
     * @param x      the center x.
     * @param y      the y.
     * @param rgb    the rgb.
     * @param shadow <code>true</code> to draw with a shadow.
     */
    void BitmapFont::DrawStringTaggableCenter(const std::string &s, const int32_t x, const int32_t y, const uint32_t rgb, const bool shadow) {
        DrawStringTaggable(s, x - (StringWidthTaggable(s) / 2), y, rgb, shadow);
    }

    /**
     * Draws a right aligned string. <b>Note:</b> This method is not taggable.
     *
     * @param s   the string.
     * @param x   the x.
     * @param y   the y.
     * @param rgb the rgb.
     */
    void BitmapFont::DrawStringRight(const std::string &s, int32_t x, int32_t y, int32_t rgb) {
        DrawString(s, x - StringWidth(s), y, rgb);
    }

    /*void BitmapFont::DrawStringCenter(const std::string &s, int32_t x, int32_t y, int32_t rgb) {
    }

    void BitmapFont::DrawStringTaggableCenter(const std::string &s, int32_t x, int32_t y, int32_t rgb, bool shadow) {
    }*/

    /**
     * Calculates the string width.
     *
     * @param s the string.
     * @return the string width.
     */
    int32_t BitmapFont::StringWidthTaggable(const std::string &s) const {
        if (s.empty()) {
            return 0;
        }
        int32_t w = 0;
        for (size_t k = 0; k < s.length(); k++) {
            if ((s[k] == '@') && ((k + 4) < s.length()) && (s[k + 4] == '@')) {
                k += 4;
            } else {
                w += charAdvance[static_cast<unsigned char>(s[k])];
            }
        }
        return w;
    }

    /**
     * Calculates the string width. <b>Note:</b> This method is not taggable.
     *
     * @param s the string.
     * @return the string width.
     */
    int32_t BitmapFont::StringWidth(const std::string &s) const {
        if (s.empty()) {
            return 0;
        }

        int32_t w = 0;
        for (unsigned char ch : s) {
            w += charAdvance[ch];  // assumes charAdvance.size() >= 256
        }
        return w;
    }

    /**
     * Standard draw string method. <b>Note:</b> This method is not taggable.
     *
     * @param s   the s.
     * @param x   the x.
     * @param y   the y.
     * @param rgb the rgb.
     */
    void BitmapFont::DrawString(const std::string &s, int32_t x, int32_t y, const uint32_t rgb) {
        if (s.empty()) {
            return;
        }

        y -= height;

        for (const unsigned char c : s) {  // ensure indexing into 0–255
            if (c != ' ') {
                FillMaskedRect(
                    charMask[c],
                    x + charOffsetX[c],
                    y + charOffsetY[c],
                    charMaskWidth[c],
                    charMaskHeight[c],
                    rgb
                );
            }
            x += charAdvance[c];
        }
    }

    void BitmapFont::DrawStringWave(const std::string& s, int32_t x, int32_t y, int32_t rgb, int32_t cycle)
    {
        if (s.empty()) {
            return;
        }
        x -= StringWidth(s) / 2;
        y -= height;
        for (int32_t i = 0; i < static_cast<int32_t>(s.length()); i++) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c != ' ') {
                int32_t waveOffset = static_cast<int32_t>(SDL_sin((static_cast<double>(i) / 2.0) + (static_cast<double>(cycle) / 5.0)) * 5.0);
                FillMaskedRect(charMask[c], x + charOffsetX[c], y + charOffsetY[c] + waveOffset, charMaskWidth[c], charMaskHeight[c], rgb);
            }
            x += charAdvance[c];
        }
    }

    void BitmapFont::DrawStringWave2(const std::string& s, int32_t x, int32_t y, int32_t rgb, int32_t cycle)
    {
        if (s.empty()) {
            return;
        }
        x -= StringWidth(s) / 2;
        y -= height;
        for (int32_t i = 0; i < static_cast<int32_t>(s.length()); i++) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c != ' ') {
                int32_t waveOffset = static_cast<int32_t>(SDL_sin((static_cast<double>(i) / 2.0) + (static_cast<double>(cycle) / 5.0)) * 5.0);
                FillMaskedRect(charMask[c], x + charOffsetX[c], y + charOffsetY[c] + waveOffset, charMaskWidth[c], charMaskHeight[c], rgb);
            }
            x += charAdvance[c];
        }
    }

    void BitmapFont::DrawStringShake(const std::string& s, int32_t x, int32_t y, int32_t rgb, int32_t cycle,
        int32_t phase)
    {
        if (s.empty()) {
            return;
        }

        double amplitude = 7.0 - (static_cast<double>(phase) / 8.0);

        if (amplitude < 0.0) {
            amplitude = 0.0;
        }

        x -= StringWidth(s) / 2;
        y -= height;

        for (int32_t i = 0; i < static_cast<int32_t>(s.length()); i++) {
            unsigned char c = static_cast<unsigned char>(s[i]);

            if (c != ' ') {
                int32_t shakeOffset = static_cast<int32_t>(SDL_sin((static_cast<double>(i) / 1.5) + static_cast<double>(cycle)) * amplitude);
                FillMaskedRect(charMask[c], x + charOffsetX[c], y + charOffsetY[c] + shakeOffset, charMaskWidth[c], charMaskHeight[c], rgb);
            }

            x += charAdvance[c];
        }
    }

    /**
     * Draws a taggable string.
     *
     * @param s        the string.
     * @param x        the x.
     * @param y        the y.
     * @param rgb      the rgb.
     * @param shadowed <code>true</code> to draw with a shadow.
     * @see #evaluateTag(String)
     */
    void BitmapFont::DrawStringTaggable(const std::string &s, int32_t x, int32_t y, uint32_t rgb, const bool shadowed) {
        strikethrough = false;

        int32_t leftX = x;

        if (s.empty()) {
            return;
        }

        y -= height;

        int32_t len = static_cast<int32_t>(s.length());
        for (int32_t i = 0; i < len; i++) {
            if ((s[i] == '@') && ((i + 4) < len) && (s[i + 4] == '@')) {
                uint32_t value = EvaluateTag(s.substr(i + 1, 3));

                if (value != -1) {
                    rgb = value;
                }

                i += 4;
            } else {
                char c = s[i];

                if (c != ' ') {
                    if (shadowed) {
                        FillMaskedRect(charMask[static_cast<unsigned char>(c)],
                                       x + charOffsetX[static_cast<unsigned char>(c)] + 1,
                                       y + charOffsetY[static_cast<unsigned char>(c)] + 1,
                                       charMaskWidth[static_cast<unsigned char>(c)],
                                       charMaskHeight[static_cast<unsigned char>(c)],
                                       0);
                    }
                    FillMaskedRect(charMask[static_cast<unsigned char>(c)],
                                   x + charOffsetX[static_cast<unsigned char>(c)],
                                   y + charOffsetY[static_cast<unsigned char>(c)],
                                   charMaskWidth[static_cast<unsigned char>(c)],
                                   charMaskHeight[static_cast<unsigned char>(c)],
                                   rgb);
                }

                x += charAdvance[static_cast<unsigned char>(c)];
            }
        }

        if (strikethrough) {
            Draw2D::DrawLineX(leftX, y + static_cast<int32_t>(static_cast<double>(height) * 0.7), x - leftX, 0x800000);
        }
    }

    /*void BitmapFont::DrawStringWave(const std::string &s, int32_t x, int32_t y, int32_t rgb, int32_t cycle) {
    }

    void BitmapFont::DrawStringWave2(const std::string &s, int32_t x, int32_t y, int32_t rgb, int32_t cycle) {
    }

    void BitmapFont::DrawStringShake(const std::string &s, int32_t x, int32_t y, int32_t rgb, int32_t cycle,
        int32_t phase) {
    }*/

    /**
     * Identical to {@link #drawStringTaggable(String, int, int, int, boolean)} with the exception of a random chance to
     * advance a character by an additional pixel to prevent macro clients from detecting tooltip text easily.
     *
     * @param s        the string.
     * @param x        the x.
     * @param y        the y.
     * @param rgb      the rgb.
     * @param shadowed <code>true</code> to draw with a shadow.
     * @param seed     the seed.
     * @see #evaluateTag(String)
     */
    void BitmapFont::DrawStringTooltip(const std::string &s, int32_t x, int32_t y, int32_t rgb, bool shadowed,
        int32_t seed) {
        /*if (s.empty()) {
            return;
        }
        random.etSeed(seed);
        int alpha = 192 + (random.nextInt() & 0x1f);
        y -= height;
        for (int i = 0; i < s.length(); i++) {
            if ((s.charAt(i) == '@') && ((i + 4) < s.length()) && (s.charAt(i + 4) == '@')) {
                int value = evaluateTag(s.substring(i + 1, i + 4));
                if (value != -1) {
                    rgb = value;
                }
                i += 4;
            } else {
                char c = s.charAt(i);
                if (c != ' ') {
                    if (shadowed) {
                        fillMaskedRect(charMask[c], x + charOffsetX[c] + 1, y + charOffsetY[c] + 1, charMaskWidth[c], charMaskHeight[c], 0, 192);
                    }
                    fillMaskedRect(charMask[c], x + charOffsetX[c], y + charOffsetY[c], charMaskWidth[c], charMaskHeight[c], rgb, alpha);
                }
                x += charAdvance[c];
                if ((random.nextInt() & 3) == 0) {
                    x++;
                }
            }
        }*/
        DrawStringTaggable(s, x, y, rgb, shadowed);
    }

    uint32_t BitmapFont::EvaluateTag(const std::string &s) {
        if (s == "red") return 0xff0000;
        if (s == "gre") return 0xff00;
        if (s == "blu") return 0xff;
        if (s == "yel") return 0xffff00;
        if (s == "cya") return 0xffff;
        if (s == "mag") return 0xff00ff;
        if (s == "whi") return 0xffffff;
        if (s == "bla") return 0;
        if (s == "lre") return 0xff9040;
        if (s == "dre") return 0x800000;
        if (s == "dbl") return 0x80;
        if (s == "or1") return 0xffb000;
        if (s == "or2") return 0xff7000;
        if (s == "or3") return 0xff3000;
        if (s == "gr1") return 0xc0ff00;
        if (s == "gr2") return 0x80ff00;
        if (s == "gr3") return 0x40ff00;

        if (s == "str") {
            strikethrough = true;
            return -1;
        }
        if (s == "end") {
            strikethrough = false;
            return -1;
        }

        return -1;
    }

    void BitmapFont::FillMaskedRect(const std::vector<int8_t> &mask, int32_t x, int32_t y, int32_t w, int32_t h,
        uint32_t rgb) {
        int32_t dstOff = x + (y * Draw2D::width);
        int32_t dstStep = Draw2D::width - w;
        int32_t maskStep = 0;
        int32_t maskOff = 0;

        if (y < Draw2D::top) {
            const int32_t trim = Draw2D::top - y;
            h -= trim;
            y = Draw2D::top;
            maskOff += trim * w;
            dstOff += trim * Draw2D::width;
        }

        if ((y + h) >= Draw2D::bottom) {
            h -= ((y + h) - Draw2D::bottom) + 1;
        }

        if (x < Draw2D::left) {
            const int32_t trim = Draw2D::left - x;
            w -= trim;
            x = Draw2D::left;
            maskOff += trim;
            dstOff += trim;
            maskStep += trim;
            dstStep += trim;
        }

        if ((x + w) >= Draw2D::right) {
            const int32_t trim = ((x + w) - Draw2D::right) + 1;
            w -= trim;
            maskStep += trim;
            dstStep += trim;
        }

        if (w > 0 && h > 0) {
            FillMaskedRect(mask, maskOff, maskStep, Draw2D::Pixels(), dstOff, dstStep, w, h, rgb);
        }
    }

    /*void BitmapFont::FillMaskedRect(const std::vector<int8_t> &mask, int32_t x, int32_t y, int32_t w, int32_t h,
        int32_t rgb, int32_t alpha) {
    }*/

    void BitmapFont::FillMaskedRect(const std::vector<int8_t>& mask, int32_t maskOff, const int32_t maskStep,
        int32_t* dst, int32_t dstOff, const int32_t dstStep, int32_t w, const int32_t h, const uint32_t rgb) {
        int32_t halfW = -(w >> 2);
        w = -(w & 3);

        for (int32_t y = -h; y < 0; y++) {
            // process groups of 4 pixels
            for (int32_t x = halfW; x < 0; x++) {
                for (int32_t i = 0; i < 4; i++) {
                    if (mask[maskOff++] != 0) {
                        dst[dstOff++] =  0xFF000000 | rgb;
                    } else {
                        dstOff++;
                    }
                }
            }

            // process remaining pixels (w mod 4)
            for (int32_t x = w; x < 0; x++) {
                if (mask[maskOff++] != 0) {
                    dst[dstOff++] =  0xFF000000 | rgb;
                } else {
                    dstOff++;
                }
            }

            dstOff += dstStep;   // move to next row in destination
            maskOff += maskStep; // move to next row in mask
        }
    }

    /*void BitmapFont::FillMaskedRect(const std::vector<int8_t> &mask, int32_t maskOff, int32_t maskStep,
        std::vector<int32_t> &dst, int32_t dstOff, int32_t dstStep, int32_t w, int32_t h, int32_t rgb, int32_t alpha) {
    }*/
}
