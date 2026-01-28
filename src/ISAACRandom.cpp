#include "ISAACRandom.h"

namespace SDL_Client
{
    ISAACRandom::ISAACRandom(const std::vector<int32_t>& seed)
    {
        std::fill_n(m_KeySetArray, SIZE, 0);
        std::fill_n(m_CryptArray, SIZE, 0);

        for (int32_t i = 0; i < 4; ++i)
            m_KeySetArray[i] = seed[i];

        InitializeKeySet();
    }

    uint32_t ISAACRandom::GetNextKey()
    {
        if (m_KeyArrayIdx-- == 0) {
            GenerateNextKeySet();
            m_KeyArrayIdx = 255;
        }
        return m_KeySetArray[m_KeyArrayIdx];
    }

    void ISAACRandom::InitializeKeySet()
    {
        constexpr uint32_t GOLDEN_RATIO = 0x9e3779b9;
        uint32_t a = GOLDEN_RATIO, b = GOLDEN_RATIO, c = GOLDEN_RATIO, d = GOLDEN_RATIO;
        uint32_t e = GOLDEN_RATIO, f = GOLDEN_RATIO, g = GOLDEN_RATIO, h = GOLDEN_RATIO;

        auto mix = [&]() {
            a ^= b << 11; d += a; b += c;
            b ^= c >> 2;  e += b; c += d;
            c ^= d << 8;  f += c; d += e;
            d ^= e >> 16; g += d; e += f;
            e ^= f << 10; h += e; f += g;
            f ^= g >> 4;  a += f; g += h;
            g ^= h << 8;  b += g; h += a;
            h ^= a >> 9;  c += h; a += b;
        };

        for (int i = 0; i < 4; ++i)
            mix();

        for (int i = 0; i < SIZE; i += 8)
        {
            a += m_KeySetArray[i];     b += m_KeySetArray[i + 1];
            c += m_KeySetArray[i + 2]; d += m_KeySetArray[i + 3];
            e += m_KeySetArray[i + 4]; f += m_KeySetArray[i + 5];
            g += m_KeySetArray[i + 6]; h += m_KeySetArray[i + 7];
            mix();
            m_CryptArray[i] = a;     m_CryptArray[i + 1] = b;
            m_CryptArray[i + 2] = c; m_CryptArray[i + 3] = d;
            m_CryptArray[i + 4] = e; m_CryptArray[i + 5] = f;
            m_CryptArray[i + 6] = g; m_CryptArray[i + 7] = h;
        }

        for (int i = 0; i < SIZE; i += 8)
        {
            a += m_CryptArray[i];     b += m_CryptArray[i + 1];
            c += m_CryptArray[i + 2]; d += m_CryptArray[i + 3];
            e += m_CryptArray[i + 4]; f += m_CryptArray[i + 5];
            g += m_CryptArray[i + 6]; h += m_CryptArray[i + 7];
            mix();
            m_CryptArray[i] = a;     m_CryptArray[i + 1] = b;
            m_CryptArray[i + 2] = c; m_CryptArray[i + 3] = d;
            m_CryptArray[i + 4] = e; m_CryptArray[i + 5] = f;
            m_CryptArray[i + 6] = g; m_CryptArray[i + 7] = h;
        }

        GenerateNextKeySet();
        m_KeyArrayIdx = SIZE;
    }

    void ISAACRandom::GenerateNextKeySet()
    {
        m_CryptVar2 += ++m_CryptVar3;

        for (int i = 0; i < SIZE; ++i)
        {
            uint32_t x = m_CryptArray[i];
            switch (i & 3) {
            case 0: m_CryptVar1 ^= (m_CryptVar1 << 13); break;
            case 1: m_CryptVar1 ^= (m_CryptVar1 >> 6);  break;
            case 2: m_CryptVar1 ^= (m_CryptVar1 << 2);  break;
            case 3: m_CryptVar1 ^= (m_CryptVar1 >> 16); break;
            }

            m_CryptVar1 += m_CryptArray[(i + 128) & 0xFF];
            uint32_t y = m_CryptArray[(x & 0x3FC) >> 2] + m_CryptVar1 + m_CryptVar2;
            m_CryptArray[i] = y;
            m_KeySetArray[i] = m_CryptVar2 = m_CryptArray[(y >> 8 & 0x3FC) >> 2] + x;
        }
    }
}
