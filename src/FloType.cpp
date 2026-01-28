#include "FloType.h"

namespace SDL_Client {

    std::vector<FloType> FloType::instances;

    void FloType::Unpack(FileArchive &archive) {
        Buffer buffer(archive.Read("flo.dat"));
        count = buffer.ReadU16();
        instances.resize(count);

        for (int32_t i = 0; i < count; i++) {
            instances[i] = FloType();
            instances[i].Read(buffer);
        }
    }

    void FloType::Read(Buffer &in) {
        while (true) {
            const int32_t code = in.ReadU8();
            if (code == 0) {
                return;
            } else if (code == 1) {
                rgb = in.Read24();
                SetColor(rgb);
            } else if (code == 2) {
                textureID = in.ReadU8();
            } else if (code == 3) {
                // In the original Java code, this case is empty.
            } else if (code == 5) {
                occludes = false;
            } else if (code == 6) {
                in.ReadString(); // Read and discard name
            } else if (code == 7) {
                const int32_t oldHue = hue;
                const int32_t oldSaturation = saturation;
                const int32_t oldLightness = lightness;
                const int32_t oldChroma = chroma;

                int32_t newRgb = in.Read24();
                SetColor(newRgb);

                // Restore original HSL values
                hue = oldHue;
                saturation = oldSaturation;
                lightness = oldLightness;
                chroma = oldChroma;
                luminance = oldChroma; // In original code, luminance is set to the old chroma
            } else {
                LOG_ERROR("Error unrecognised flo config code: %i", code);
            }
        }
    }

    void FloType::SetColor(int32_t newRgb) {
        double red = static_cast<double>((newRgb >> 16) & 0xff) / 256.0;
        double green = static_cast<double>((newRgb >> 8) & 0xff) / 256.0;
        double blue = static_cast<double>(newRgb & 0xff) / 256.0;

        const double min = std::min({red, green, blue});
        const double max = std::max({red, green, blue});

        double h = 0.0;
        double s = 0.0;
        const double l = (min + max) / 2.0;

        if (min != max) {
            if (l < 0.5) {
                s = (max - min) / (max + min);
            } else {
                s = (max - min) / (2.0 - max - min);
            }

            if (red == max) {
                h = (green - blue) / (max - min);
            } else if (green == max) {
                h = 2.0 + (blue - red) / (max - min);
            } else { // blue == max
                h = 4.0 + (red - green) / (max - min);
            }
        }

        h /= 6.0;

        hue = static_cast<int32_t>(h * 256.0);
        saturation = static_cast<int32_t>(s * 256.0);
        lightness = static_cast<int32_t>(l * 256.0);

        saturation = std::clamp(saturation, 0, 255);
        lightness = std::clamp(lightness, 0, 255);

        if (l > 0.5) {
            luminance = static_cast<int32_t>((1.0 - l) * s * 512.0);
        } else {
            luminance = static_cast<int32_t>(l * s * 512.0);
        }
        luminance = std::max(1, luminance);

        chroma = static_cast<int32_t>(h * static_cast<double>(luminance));

        // C++ random number generation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> hue_dist(-8, 8);
        std::uniform_int_distribution<> sat_light_dist(-24, 24);

        const int32_t temp_hue = std::clamp(hue + hue_dist(gen), 0, 255);
        const int32_t temp_saturation = std::clamp(saturation + sat_light_dist(gen), 0, 255);
        const int32_t temp_lightness = std::clamp(lightness + sat_light_dist(gen), 0, 255);

        hsl = DecimateHSL(temp_hue, temp_saturation, temp_lightness);
    }

    int32_t FloType::DecimateHSL(int32_t hue, int32_t saturation, int32_t lightness) {
        if (lightness > 243) {
            saturation /= 4;
        } else if (lightness > 217) {
            saturation /= 3; // Simplified from two divisions
        } else if (lightness > 192) {
            saturation /= 2;
        } else if (lightness > 179) {
            saturation /= 2;
        }
        return ((hue / 4) << 10) + ((saturation / 32) << 7) + (lightness / 2);
    }
}
