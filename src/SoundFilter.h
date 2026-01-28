#pragma once

#include "PCH.h"
#include "Buffer.h"

namespace SDL_Client
{
    class SoundEnvelope;

    class SoundFilter
    {
    public:
        SoundFilter() = default;

        float Gain(int32_t direction, int32_t pair, float delta);
        float Normalize(float f);
        float Phase(int32_t direction, int32_t pair, float delta);
        int32_t Evaluate(int32_t direction, float delta);
        void Read(Buffer& in, SoundEnvelope& envelope);

    public:
        inline static float coefficient[2][8] = {};
        inline static int32_t coefficient16[2][8] = {};
        inline static float unity = 0.0f;
        inline static int32_t unity16 = 0;

    public:
        int32_t pairs[2] = {0, 0};
        int32_t frequencies[2][2][4] = {};
        int32_t ranges[2][2][4] = {};
        int32_t unities[2] = {0, 0};
    };
}