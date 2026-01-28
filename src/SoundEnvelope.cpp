#include "SoundEnvelope.h"

namespace SDL_Client
{
    void SoundEnvelope::Read(Buffer& in)
    {
        form = in.ReadU8();
        start = in.Read32();
        end = in.Read32();
        ReadShape(in);
    }

    void SoundEnvelope::ReadShape(Buffer& buffer)
    {
        length = buffer.ReadU8();
        shapeDelta.resize(length);
        shapePeak.resize(length);
        for (int32_t i = 0; i < length; i++)
        {
            shapeDelta[i] = buffer.ReadU16();
            shapePeak[i] = buffer.ReadU16();
        }
    }

    void SoundEnvelope::Reset()
    {
        threshold = 0;
        position = 0;
        delta = 0;
        amplitude = 0;
        ticks = 0;
    }

    int32_t SoundEnvelope::Evaluate(int32_t delta)
    {
        if (ticks >= threshold)
        {
            amplitude = shapePeak[position++] << 15;
            if (position >= length)
            {
                position = length - 1;
            }
            threshold = static_cast<int32_t>((static_cast<double>(shapeDelta[position]) / 65536.0) * static_cast<double>(delta));
            if (threshold > ticks)
            {
                this->delta = ((shapePeak[position] << 15) - amplitude) / (threshold - ticks);
            }
        }
        amplitude += this->delta;
        ticks++;
        return (amplitude - this->delta) >> 15;
    }
}