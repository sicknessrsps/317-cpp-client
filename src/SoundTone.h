#pragma once

#include "PCH.h"
#include "Buffer.h"

namespace SDL_Client
{
    class SoundEnvelope;
    class SoundFilter;

    class SoundTone
    {
    public:
        static void Init();
        std::vector<int32_t> Generate(int32_t sampleCount, int32_t length);
        int32_t Generate(int32_t form, int32_t phase, int32_t amplitude);
        void Read(Buffer& in);

    public:
        inline static std::array<int32_t, 5> tmpPhases{};
        inline static std::array<int32_t, 5> tmpDelays{};
        inline static std::array<int32_t, 5> tmpVolumes{};
        inline static std::array<int32_t, 5> tmpSemitones{};
        inline static std::array<int32_t, 5> tmpStarts{};
        inline static std::vector<int32_t> buffer;
        inline static std::vector<int32_t> noise;
        inline static std::vector<int32_t> sin;

    public:
        std::array<int32_t, 5> harmonicVolume{};
        std::array<int32_t, 5> harmonicSemitone{};
        std::array<int32_t, 5> harmonicDelay{};
        std::shared_ptr<SoundEnvelope> frequencyBase;
        std::shared_ptr<SoundEnvelope> amplitudeBase;
        std::shared_ptr<SoundEnvelope> frequencyModRate;
        std::shared_ptr<SoundEnvelope> frequencyModRange;
        std::shared_ptr<SoundEnvelope> amplitudeModRate;
        std::shared_ptr<SoundEnvelope> amplitudeModRange;
        std::shared_ptr<SoundEnvelope> release;
        std::shared_ptr<SoundEnvelope> attack;
        int32_t reverbDelay = 0;
        int32_t reverbVolume = 100;
        std::shared_ptr<SoundFilter> filter;
        std::shared_ptr<SoundEnvelope> filterRange;
        int32_t length = 500;
        int32_t start = 0;
    };
}