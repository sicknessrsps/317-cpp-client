#include "SoundTone.h"
#include "SoundEnvelope.h"
#include "SoundFilter.h"

namespace SDL_Client
{
    void SoundTone::Init()
    {
        noise.resize(32768);

        for (int32_t i = 0; i < 32768; i++)
        {
            if (SDL_randf() > 0.5f)
            {
                noise[i] = 1;
            }
            else
            {
                noise[i] = -1;
            }
        }

        sin.resize(32768);
        for (int32_t j = 0; j < 32768; j++)
        {
            sin[j] = static_cast<int32_t>(std::sin(static_cast<double>(j) / 5215.1903) * 16384.0);
        }

        buffer.resize(22050 * 10); // 10 second buffer
    }

    std::vector<int32_t> SoundTone::Generate(int32_t sampleCount, int32_t length)
    {
        for (int32_t sample = 0; sample < sampleCount; sample++)
        {
            buffer[sample] = 0;
        }

        if (length < 10)
        {
            return buffer;
        }

        double samplesPerStep = static_cast<double>(sampleCount) / (static_cast<double>(length) + 0.0);

        frequencyBase->Reset();
        amplitudeBase->Reset();

        int32_t frequencyStart = 0;
        int32_t frequencyDuration = 0;
        int32_t frequencyPhase = 0;

        if (frequencyModRate != nullptr)
        {
            frequencyModRate->Reset();
            frequencyModRange->Reset();
            frequencyStart = static_cast<int32_t>((static_cast<double>(frequencyModRate->end - frequencyModRate->start) * 32.768) / samplesPerStep);
            frequencyDuration = static_cast<int32_t>((static_cast<double>(frequencyModRate->start) * 32.768) / samplesPerStep);
        }

        int32_t amplitudeStart = 0;
        int32_t amplitudeDuration = 0;
        int32_t amplitudePhase = 0;

        if (amplitudeModRate != nullptr)
        {
            amplitudeModRate->Reset();
            amplitudeModRange->Reset();
            amplitudeStart = static_cast<int32_t>((static_cast<double>(amplitudeModRate->end - amplitudeModRate->start) * 32.768) / samplesPerStep);
            amplitudeDuration = static_cast<int32_t>((static_cast<double>(amplitudeModRate->start) * 32.768) / samplesPerStep);
        }

        for (int32_t harmonic = 0; harmonic < 5; harmonic++)
        {
            if (harmonicVolume[harmonic] != 0)
            {
                tmpPhases[harmonic] = 0;
                tmpDelays[harmonic] = static_cast<int32_t>(static_cast<double>(harmonicDelay[harmonic]) * samplesPerStep);
                tmpVolumes[harmonic] = (harmonicVolume[harmonic] << 14) / 100;
                tmpSemitones[harmonic] = static_cast<int32_t>((static_cast<double>(frequencyBase->end - frequencyBase->start) * 32.768 * std::pow(1.0057929410678534, harmonicSemitone[harmonic])) / samplesPerStep);
                tmpStarts[harmonic] = static_cast<int32_t>((static_cast<double>(frequencyBase->start) * 32.768) / samplesPerStep);
            }
        }

        for (int32_t sample = 0; sample < sampleCount; sample++)
        {
            int32_t frequency = frequencyBase->Evaluate(sampleCount);
            int32_t amplitude = amplitudeBase->Evaluate(sampleCount);

            if (frequencyModRate != nullptr)
            {
                int32_t rate = frequencyModRate->Evaluate(sampleCount);
                int32_t range = frequencyModRange->Evaluate(sampleCount);
                frequency += Generate(frequencyModRate->form, frequencyPhase, range) >> 1;
                frequencyPhase += ((rate * frequencyStart) >> 16) + frequencyDuration;
            }

            if (amplitudeModRate != nullptr)
            {
                int32_t rate = amplitudeModRate->Evaluate(sampleCount);
                int32_t range = amplitudeModRange->Evaluate(sampleCount);
                amplitude = (amplitude * ((Generate(amplitudeModRate->form, amplitudePhase, range) >> 1) + 32768)) >> 15;
                amplitudePhase += ((rate * amplitudeStart) >> 16) + amplitudeDuration;
            }

            for (int32_t harmonic = 0; harmonic < 5; harmonic++)
            {
                if (harmonicVolume[harmonic] != 0)
                {
                    int32_t position = sample + tmpDelays[harmonic];

                    if (position < sampleCount)
                    {
                        buffer[position] += Generate(frequencyBase->form, tmpPhases[harmonic], (amplitude * tmpVolumes[harmonic]) >> 15);
                        tmpPhases[harmonic] += ((frequency * tmpSemitones[harmonic]) >> 16) + tmpStarts[harmonic];
                    }
                }
            }
        }

        if (release != nullptr)
        {
            release->Reset();
            attack->Reset();

            int32_t counter = 0;
            bool muted = true;

            for (int32_t sample = 0; sample < sampleCount; sample++)
            {
                int32_t releaseValue = release->Evaluate(sampleCount);
                int32_t attackValue = attack->Evaluate(sampleCount);
                int32_t threshold;

                if (muted)
                {
                    threshold = release->start + (((release->end - release->start) * releaseValue) >> 8);
                }
                else
                {
                    threshold = release->start + (((release->end - release->start) * attackValue) >> 8);
                }

                if ((counter += 256) >= threshold)
                {
                    counter = 0;
                    muted = !muted;
                }

                if (muted)
                {
                    buffer[sample] = 0;
                }
            }
        }

        if ((reverbDelay > 0) && (reverbVolume > 0))
        {
            int32_t start = static_cast<int32_t>(static_cast<double>(reverbDelay) * samplesPerStep);

            for (int32_t sample = start; sample < sampleCount; sample++)
            {
                buffer[sample] += (buffer[sample - start] * reverbVolume) / 100;
            }
        }

        if ((filter->pairs[0] > 0) || (filter->pairs[1] > 0))
        {
            filterRange->Reset();

            int32_t range = filterRange->Evaluate(sampleCount + 1);
            int32_t forward = filter->Evaluate(0, static_cast<float>(range) / 65536.0f);
            int32_t backward = filter->Evaluate(1, static_cast<float>(range) / 65536.0f);

            if (sampleCount >= (forward + backward))
            {
                int32_t index = 0;
                int32_t interval = backward;

                if (interval > (sampleCount - forward))
                {
                    interval = sampleCount - forward;
                }

                for (; index < interval; index++)
                {
                    int32_t sample = static_cast<int32_t>((static_cast<int64_t>(buffer[index + forward]) * static_cast<int64_t>(SoundFilter::unity16)) >> 16);

                    for (int32_t offset = 0; offset < forward; offset++)
                    {
                        sample += static_cast<int32_t>((static_cast<int64_t>(buffer[(index + forward) - 1 - offset]) * static_cast<int64_t>(SoundFilter::coefficient16[0][offset])) >> 16);
                    }

                    for (int32_t offset = 0; offset < index; offset++)
                    {
                        sample -= static_cast<int32_t>((static_cast<int64_t>(buffer[index - 1 - offset]) * static_cast<int64_t>(SoundFilter::coefficient16[1][offset])) >> 16);
                    }

                    buffer[index] = sample;
                    range = filterRange->Evaluate(sampleCount + 1);
                }

                interval = 128;

                do
                {
                    if (interval > (sampleCount - forward))
                    {
                        interval = sampleCount - forward;
                    }

                    for (; index < interval; index++)
                    {
                        int32_t sample = static_cast<int32_t>((static_cast<int64_t>(buffer[index + forward]) * static_cast<int64_t>(SoundFilter::unity16)) >> 16);

                        for (int32_t offset = 0; offset < forward; offset++)
                        {
                            sample += static_cast<int32_t>((static_cast<int64_t>(buffer[(index + forward) - 1 - offset]) * static_cast<int64_t>(SoundFilter::coefficient16[0][offset])) >> 16);
                        }

                        for (int32_t offset = 0; offset < backward; offset++)
                        {
                            sample -= static_cast<int32_t>((static_cast<int64_t>(buffer[index - 1 - offset]) * static_cast<int64_t>(SoundFilter::coefficient16[1][offset])) >> 16);
                        }

                        buffer[index] = sample;
                        range = filterRange->Evaluate(sampleCount + 1);
                    }

                    if (index >= (sampleCount - forward))
                    {
                        break;
                    }

                    forward = filter->Evaluate(0, static_cast<float>(range) / 65536.0f);
                    backward = filter->Evaluate(1, static_cast<float>(range) / 65536.0f);
                    interval += 128;
                } while (true);

                for (; index < sampleCount; index++)
                {
                    int32_t sample = 0;

                    for (int32_t offset = (index + forward) - sampleCount; offset < forward; offset++)
                    {
                        sample += static_cast<int32_t>((static_cast<int64_t>(buffer[(index + forward) - 1 - offset]) * static_cast<int64_t>(SoundFilter::coefficient16[0][offset])) >> 16);
                    }

                    for (int32_t offset = 0; offset < backward; offset++)
                    {
                        sample -= static_cast<int32_t>((static_cast<int64_t>(buffer[index - 1 - offset]) * static_cast<int64_t>(SoundFilter::coefficient16[1][offset])) >> 16);
                    }

                    buffer[index] = sample;
                    filterRange->Evaluate(sampleCount + 1);
                }
            }
        }

        for (int32_t sample = 0; sample < sampleCount; sample++)
        {
            if (buffer[sample] < -32768)
            {
                buffer[sample] = -32768;
            }
            if (buffer[sample] > 32767)
            {
                buffer[sample] = 32767;
            }
        }

        return buffer;
    }

    int32_t SoundTone::Generate(int32_t form, int32_t phase, int32_t amplitude)
    {
        if (form == 1)
        {
            if ((phase & 0x7fff) < 16384)
            {
                return amplitude;
            }
            else
            {
                return -amplitude;
            }
        }
        else if (form == 2)
        {
            return (sin[phase & 0x7fff] * amplitude) >> 14;
        }
        else if (form == 3)
        {
            return (((phase & 0x7fff) * amplitude) >> 14) - amplitude;
        }
        else if (form == 4)
        {
            return noise[(phase / 2607) & 0x7fff] * amplitude;
        }
        else
        {
            return 0;
        }
    }

    void SoundTone::Read(Buffer& in)
    {
        frequencyBase = std::make_shared<SoundEnvelope>();
        frequencyBase->Read(in);
        amplitudeBase = std::make_shared<SoundEnvelope>();
        amplitudeBase->Read(in);

        if (in.ReadU8() != 0)
        {
            in.position--;
            frequencyModRate = std::make_shared<SoundEnvelope>();
            frequencyModRate->Read(in);
            frequencyModRange = std::make_shared<SoundEnvelope>();
            frequencyModRange->Read(in);
        }

        if (in.ReadU8() != 0)
        {
            in.position--;
            amplitudeModRate = std::make_shared<SoundEnvelope>();
            amplitudeModRate->Read(in);
            amplitudeModRange = std::make_shared<SoundEnvelope>();
            amplitudeModRange->Read(in);
        }

        if (in.ReadU8() != 0)
        {
            in.position--;
            release = std::make_shared<SoundEnvelope>();
            release->Read(in);
            attack = std::make_shared<SoundEnvelope>();
            attack->Read(in);
        }

        for (int32_t i = 0; i < 10; i++)
        {
            int32_t volume = in.ReadUSmart();

            if (volume == 0)
            {
                break;
            }

            harmonicVolume[i] = volume;
            harmonicSemitone[i] = in.ReadSmart();
            harmonicDelay[i] = in.ReadUSmart();
        }

        reverbDelay = in.ReadUSmart();
        reverbVolume = in.ReadUSmart();

        length = in.ReadU16();
        start = in.ReadU16();

        filter = std::make_shared<SoundFilter>();
        filterRange = std::make_shared<SoundEnvelope>();
        filter->Read(in, *filterRange);
    }
}