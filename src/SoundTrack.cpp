#include "SoundTrack.h"
#include "SoundTone.h"

namespace SDL_Client
{
    void SoundTrack::Unpack(Buffer& in)
    {
        SoundTone::Init();

        while (true)
        {
            int32_t id = in.ReadU16();

            if (id == 65535)
            {
                return;
            }

            tracks[id] = std::make_shared<SoundTrack>();
            tracks[id]->Read(in);
            delays[id] = tracks[id]->Trim();
        }
    }

    Buffer* SoundTrack::Generate(int32_t loopCount, int32_t id)
    {
        if (tracks[id] != nullptr)
        {
            SoundTrack* track = tracks[id].get();
            return track->GetWave(loopCount);
        }
        else
        {
            return nullptr;
        }
    }

    void SoundTrack::Read(Buffer& in)
    {
        for (int32_t tone = 0; tone < 10; tone++)
        {
            if (in.ReadU8() != 0)
            {
                in.position--;
                tones[tone] = std::make_shared<SoundTone>();
                tones[tone]->Read(in);
            }
        }

        loopBegin = in.ReadU16();
        loopEnd = in.ReadU16();
    }

    int32_t SoundTrack::Trim()
    {
        int32_t start = 9999999;

        for (int32_t tone = 0; tone < 10; tone++)
        {
            if ((tones[tone] != nullptr) && ((tones[tone]->start / 20) < start))
            {
                start = tones[tone]->start / 20;
            }
        }

        if ((loopBegin < loopEnd) && ((loopBegin / 20) < start))
        {
            start = loopBegin / 20;
        }

        if ((start == 9999999) || (start == 0))
        {
            return 0;
        }

        for (int32_t tone = 0; tone < 10; tone++)
        {
            if (tones[tone] != nullptr)
            {
                tones[tone]->start -= start * 20;
            }
        }

        if (loopBegin < loopEnd)
        {
            loopBegin -= start * 20;
            loopEnd -= start * 20;
        }

        return start;
    }

    Buffer* SoundTrack::GetWave(int32_t loopCount)
    {
        int32_t length = Generate(loopCount);
        waveBuffer.position = 0;
        waveBuffer.Write32(0x52494646);     // "RIFF" ChunkID
        waveBuffer.Write32LE(36 + length);  // ChunkSize
        waveBuffer.Write32(0x57415645);     // "WAVE" format
        waveBuffer.Write32(0x666d7420);     // "fmt " chunk id
        waveBuffer.Write32LE(16);           // chunk size
        waveBuffer.Write16LE(1);            // audio format
        waveBuffer.Write16LE(1);            // num channels
        waveBuffer.Write32LE(22050);        // sample rate
        waveBuffer.Write32LE(22050);        // byte rate
        waveBuffer.Write16LE(1);            // block align
        waveBuffer.Write16LE(8);            // bits per sample
        waveBuffer.Write32(0x64617461);     // "data"
        waveBuffer.Write32LE(length);

        // Copy generated audio data from waveBytes to waveBuffer.data
        // (waveBuffer.data is a copy, not a reference to waveBytes)
        std::copy(waveBytes.begin() + 44, waveBytes.begin() + 44 + length, waveBuffer.data.begin() + 44);

        waveBuffer.position += length;
        return &waveBuffer;
    }

    int32_t SoundTrack::Generate(int32_t loopCount)
    {
        int32_t duration = 0;

        for (int32_t tone = 0; tone < 10; tone++)
        {
            if ((tones[tone] != nullptr) && ((tones[tone]->length + tones[tone]->start) > duration))
            {
                duration = tones[tone]->length + tones[tone]->start;
            }
        }

        if (duration == 0)
        {
            return 0;
        }

        int32_t sampleCount = (22050 * duration) / 1000;
        int32_t loopStart = (22050 * loopBegin) / 1000;
        int32_t loopStop = (22050 * loopEnd) / 1000;

        if ((loopStart < 0) || (loopStop < 0) || (loopStop > sampleCount) || (loopStart >= loopStop))
        {
            loopCount = 0;
        }

        int32_t totalSampleCount = sampleCount + ((loopStop - loopStart) * (loopCount - 1));

        for (int32_t sample = 44; sample < (totalSampleCount + 44); sample++)
        {
            waveBytes[sample] = -128;
        }

        for (int32_t tone = 0; tone < 10; tone++)
        {
            if (tones[tone] == nullptr)
            {
                continue;
            }

            int32_t toneSampleCount = (tones[tone]->length * 22050) / 1000;
            int32_t start = (tones[tone]->start * 22050) / 1000;
            std::vector<int32_t> samples = tones[tone]->Generate(toneSampleCount, tones[tone]->length);

            for (int32_t sample = 0; sample < toneSampleCount; sample++)
            {
                waveBytes[sample + start + 44] += static_cast<int8_t>(samples[sample] >> 8);
            }
        }

        if (loopCount > 1)
        {
            // All of these 44's are because we're avoiding the area where the WAV header will be written.
            loopStart += 44;
            loopStop += 44;
            sampleCount += 44;

            // Moves the end of the sound (after the loops) to the true end of the buffer.
            int32_t endOffset = (totalSampleCount += 44) - sampleCount;
            for (int32_t sample = sampleCount - 1; sample >= loopStop; sample--)
            {
                waveBytes[sample + endOffset] = waveBytes[sample];
            }

            // Duplicates loop area.
            for (int32_t loop = 1; loop < loopCount; loop++)
            {
                int32_t offset = (loopStop - loopStart) * loop;
                for (int32_t sample = loopStart; sample < loopStop; sample++)
                {
                    waveBytes[sample + offset] = waveBytes[sample];
                }
            }

            totalSampleCount -= 44;
        }

        return totalSampleCount;
    }
}