#pragma once
#include "PCH.h"

namespace SDL_Client
{
    class ISAACRandom {
    public:
        explicit ISAACRandom(const std::vector<int32_t>& seed);
        ISAACRandom() = default;
        ~ISAACRandom() = default;

        uint32_t GetNextKey();

    private:
        void InitializeKeySet();
        void GenerateNextKeySet();
    private:

        static constexpr int SIZE = 256;
        uint32_t m_KeySetArray[SIZE]{};
        uint32_t m_CryptArray[SIZE]{};

        uint32_t m_CryptVar1 = 0;
        uint32_t m_CryptVar2 = 0;
        uint32_t m_CryptVar3 = 0;
        uint32_t m_KeyArrayIdx = 0;
    };
}
