#include "SoundFilter.h"
#include "SoundEnvelope.h"

namespace SDL_Client
{
    float SoundFilter::Gain(int32_t direction, int32_t pair, float delta)
    {
        float g = static_cast<float>(ranges[direction][0][pair]) + (delta * static_cast<float>(ranges[direction][1][pair] - ranges[direction][0][pair]));
        g *= 0.001525879f;
        return 1.0f - static_cast<float>(std::pow(10.0, -g / 20.0f));
    }

    float SoundFilter::Normalize(float f)
    {
        return (32.7032f * static_cast<float>(std::pow(2.0, f)) * 3.141593f) / 11025.0f;
    }

    float SoundFilter::Phase(int32_t direction, int32_t pair, float delta)
    {
        float f1 = static_cast<float>(frequencies[direction][0][pair]) + (delta * static_cast<float>(frequencies[direction][1][pair] - frequencies[direction][0][pair]));
        f1 *= 0.0001220703f;
        return Normalize(f1);
    }

    int32_t SoundFilter::Evaluate(int32_t direction, float delta)
    {
        if (direction == 0)
        {
            float u = static_cast<float>(unities[0]) + (static_cast<float>(unities[1] - unities[0]) * delta);
            u *= 0.003051758f;
            unity = static_cast<float>(std::pow(0.1, u / 20.0f));
            unity16 = static_cast<int32_t>(unity * 65536.0f);
        }

        if (pairs[direction] == 0)
        {
            return 0;
        }

        float u = Gain(direction, 0, delta);

        coefficient[direction][0] = -2.0f * u * std::cos(Phase(direction, 0, delta));
        coefficient[direction][1] = u * u;

        for (int32_t pair = 1; pair < pairs[direction]; pair++)
        {
            float g = Gain(direction, pair, delta);
            float a = -2.0f * g * std::cos(Phase(direction, pair, delta));
            float b = g * g;

            coefficient[direction][(pair * 2) + 1] = coefficient[direction][(pair * 2) - 1] * b;
            coefficient[direction][pair * 2] = (coefficient[direction][(pair * 2) - 1] * a) + (coefficient[direction][(pair * 2) - 2] * b);

            for (int32_t j = (pair * 2) - 1; j >= 2; j--)
            {
                coefficient[direction][j] += (coefficient[direction][j - 1] * a) + (coefficient[direction][j - 2] * b);
            }

            coefficient[direction][1] += (coefficient[direction][0] * a) + b;
            coefficient[direction][0] += a;
        }

        if (direction == 0)
        {
            for (int32_t l = 0; l < (pairs[0] * 2); l++)
            {
                coefficient[0][l] *= unity;
            }
        }

        for (int32_t pair = 0; pair < (pairs[direction] * 2); pair++)
        {
            coefficient16[direction][pair] = static_cast<int32_t>(coefficient[direction][pair] * 65536.0f);
        }

        return pairs[direction] * 2;
    }

    void SoundFilter::Read(Buffer& in, SoundEnvelope& envelope)
    {
        int32_t count = in.ReadU8();
        pairs[0] = count >> 4;
        pairs[1] = count & 0xf;

        if (count != 0)
        {
            unities[0] = in.ReadU16();
            unities[1] = in.ReadU16();

            int32_t migration = in.ReadU8();

            for (int32_t direction = 0; direction < 2; direction++)
            {
                for (int32_t pair = 0; pair < pairs[direction]; pair++)
                {
                    frequencies[direction][0][pair] = in.ReadU16();
                    ranges[direction][0][pair] = in.ReadU16();
                }
            }

            for (int32_t direction = 0; direction < 2; direction++)
            {
                for (int32_t pair = 0; pair < pairs[direction]; pair++)
                {
                    if ((migration & (1 << (direction * 4) << pair)) != 0)
                    {
                        frequencies[direction][1][pair] = in.ReadU16();
                        ranges[direction][1][pair] = in.ReadU16();
                    }
                    else
                    {
                        frequencies[direction][1][pair] = frequencies[direction][0][pair];
                        ranges[direction][1][pair] = ranges[direction][0][pair];
                    }
                }
            }

            if ((migration != 0) || (unities[1] != unities[0]))
            {
                envelope.ReadShape(in);
            }
        }
        else
        {
            unities[0] = unities[1] = 0;
        }
    }
}