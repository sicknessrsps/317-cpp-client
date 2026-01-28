#pragma once

#include "PCH.h"
#include "Buffer.h"

namespace SDL_Client
{
    class SoundEnvelope
    {
    public:
        SoundEnvelope() = default;

        void Read(Buffer& in);
        void ReadShape(Buffer& buffer);
        void Reset();
        int32_t Evaluate(int32_t delta);

    public:
        int32_t length = 0;
        std::vector<int32_t> shapeDelta;
        std::vector<int32_t> shapePeak;
        int32_t start = 0;
        int32_t end = 0;
        int32_t form = 0;
        int32_t threshold = 0;
        int32_t position = 0;
        int32_t delta = 0;
        int32_t amplitude = 0;
        int32_t ticks = 0;
    };
}