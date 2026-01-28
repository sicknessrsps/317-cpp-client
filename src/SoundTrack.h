#pragma once

#include "PCH.h"
#include "Buffer.h"

namespace SDL_Client
{
    class SoundTone;

    class SoundTrack
    {
    public:
        static void Unpack(Buffer& in);
        static Buffer* Generate(int32_t loopCount, int32_t id);

    public:
        SoundTrack() = default;
        void Read(Buffer& in);
        int32_t Trim();
        Buffer* GetWave(int32_t loopCount);
        int32_t Generate(int32_t loopCount);

    public:
        inline static std::vector<std::shared_ptr<SoundTrack>> tracks = std::vector<std::shared_ptr<SoundTrack>>(5000);
        inline static std::vector<int32_t> delays = std::vector<int32_t>(5000);
        inline static std::vector<int8_t> waveBytes = std::vector<int8_t>(441000);
        inline static Buffer waveBuffer = Buffer(waveBytes);

    public:
        std::vector<std::shared_ptr<SoundTone>> tones = std::vector<std::shared_ptr<SoundTone>>(10);
        int32_t loopBegin = 0;
        int32_t loopEnd = 0;
    };
}