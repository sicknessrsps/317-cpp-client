#include "AudioManager.h"
#include "Signlink.h"
#include "SoundTrack.h"

// TinySoundFont implementation
#define TSF_IMPLEMENTATION
#include "tsf.h"
#define TML_IMPLEMENTATION
#include "tml.h"

namespace SDL_Client
{
    AudioManager::~AudioManager()
    {
        Shutdown();
    }

    bool AudioManager::Init()
    {
        if (initialized)
        {
            return true;
        }

        // Create SDL mutex for thread safety
        audioMutex = SDL_CreateMutex();
        if (audioMutex == nullptr)
        {
            LOG_ERROR("Failed to create audio mutex: %s", SDL_GetError());
            return false;
        }

        // Initialize SDL audio subsystem
        if (!SDL_WasInit(SDL_INIT_AUDIO))
        {
            if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
            {
                LOG_ERROR("Failed to initialize SDL audio: %s", SDL_GetError());
                SDL_DestroyMutex(audioMutex);
                audioMutex = nullptr;
                return false;
            }
        }

        // Define the audio format we want
        SDL_AudioSpec spec;
        spec.freq = SAMPLE_RATE;
        spec.format = SDL_AUDIO_F32;
        spec.channels = CHANNELS;

        // Open the default audio device
        audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, this);
        if (audioStream == nullptr)
        {
            LOG_ERROR("Failed to open audio device: %s", SDL_GetError());
            SDL_DestroyMutex(audioMutex);
            audioMutex = nullptr;
            return false;
        }

        // Get the device ID from the stream
        audioDevice = SDL_GetAudioStreamDevice(audioStream);

        // Resume the audio device (starts calling the callback)
        SDL_ResumeAudioStreamDevice(audioStream);

        initialized = true;

        // Load the soundfont from the resolved cache directory
        if (!Signlink::soundFontPath.empty())
        {
            LoadSoundFont(Signlink::soundFontPath.string().c_str());
        }

        return true;
    }

    void AudioManager::Shutdown()
    {
        if (!initialized)
        {
            return;
        }

        // Stop MIDI playback
        StopMIDI();

        // Close the audio stream (this also closes the device)
        if (audioStream != nullptr)
        {
            SDL_DestroyAudioStream(audioStream);
            audioStream = nullptr;
            audioDevice = 0;
        }

        // Free render buffer
        std::vector<float>().swap(renderBuffer);

        // Free SoundFont
        if (soundFont != nullptr)
        {
            tsf_close(soundFont);
            soundFont = nullptr;
        }

        // Destroy mutex
        if (audioMutex != nullptr)
        {
            SDL_DestroyMutex(audioMutex);
            audioMutex = nullptr;
        }

        initialized = false;
    }

    void AudioManager::AudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
    {
        AudioManager* manager = static_cast<AudioManager*>(userdata);

        // Calculate number of samples needed (stereo, so divide by 2 channels and 4 bytes per float)
        int samples = additional_amount / (CHANNELS * sizeof(float));
        if (samples <= 0)
        {
            return;
        }

        // Reuse pre-allocated buffer to avoid per-callback heap allocation
        size_t needed = static_cast<size_t>(samples) * CHANNELS;
        if (manager->renderBuffer.size() < needed)
        {
            manager->renderBuffer.resize(needed);
        }
        std::memset(manager->renderBuffer.data(), 0, needed * sizeof(float));

        // Render audio
        manager->RenderAudio(manager->renderBuffer.data(), samples);

        // Put the rendered audio into the stream
        SDL_PutAudioStreamData(stream, manager->renderBuffer.data(), samples * CHANNELS * sizeof(float));
    }

    void AudioManager::RenderAudio(float* buffer, int samples)
    {
        // Lock mutex to safely access audio state
        SDL_LockMutex(audioMutex);

        // Calculate time per sample in milliseconds
        const double msPerSample = 1000.0 / SAMPLE_RATE;

        // Process fade timing (check if we need to advance fade step)
        if (midiIsFading && hasPendingMidi)
        {
            uint64_t currentTime = SDL_GetTicks();
            while (currentTime >= fadeLastStepTime + FADE_STEP_MS && fadeStep < FADE_STEPS)
            {
                fadeStep++;
                fadeLastStepTime += FADE_STEP_MS;

                // Check if fade is complete
                if (fadeStep >= FADE_STEPS)
                {
                    // Fade complete - switch to pending MIDI
                    SwitchToPendingMIDI();
                    break;
                }
            }
        }

        // Process audio in small blocks for accurate MIDI timing
        constexpr int BLOCK_SIZE = 64;
        float* outputPtr = buffer;
        int samplesRemaining = samples;

        while (samplesRemaining > 0)
        {
            int blockSamples = std::min(samplesRemaining, BLOCK_SIZE);

            // Process MIDI messages up to current time
            if (midiIsPlaying && soundFont != nullptr && midiMessageCurrent != nullptr)
            {
                double blockEndTime = midiTime + (blockSamples * msPerSample);
                ProcessMIDIMessages(blockEndTime);
                midiTime = blockEndTime;
            }

            // Render MIDI audio if playing
            if (midiIsPlaying && soundFont != nullptr)
            {
                tsf_render_float(soundFont, outputPtr, blockSamples, 0);

                // Calculate effective volume
                float volume = midiGain;

                // Apply fade attenuation if fading out
                if (midiIsFading)
                {
                    // Each fade step reduces volume by 100 millibels (-1 dB)
                    // fadeStep goes from 0 to 36, so volume reduction is 0 to -3600 millibels
                    int32_t fadeMillibels = -(fadeStep * 100);
                    // Convert millibels to linear gain: 10^(millibels/2000)
                    float fadeAttenuation = std::pow(10.0f, fadeMillibels / 2000.0f);
                    volume *= fadeAttenuation;
                }

                // Apply volume to rendered samples
                for (int i = 0; i < blockSamples * CHANNELS; i++)
                {
                    outputPtr[i] *= volume;
                }
            }

            // Mix in wave/sound effects if playing
            if (waveIsPlaying && !currentWaveSamples.empty())
            {
                // currentWaveSamples contains upsampled mono samples at 44100 Hz
                // We need to output stereo, so each sample goes to both L and R channels
                for (int i = 0; i < blockSamples; i++)
                {
                    if (waveSamplePosition < currentWaveSamples.size())
                    {
                        float sample = currentWaveSamples[waveSamplePosition] * soundGain;
                        // Output to both left and right channels (stereo)
                        outputPtr[i * CHANNELS] += sample;      // Left
                        outputPtr[i * CHANNELS + 1] += sample;  // Right
                        waveSamplePosition++;
                    }
                    else
                    {
                        waveIsPlaying = false;
                        break;
                    }
                }
            }

            outputPtr += blockSamples * CHANNELS;
            samplesRemaining -= blockSamples;
        }

        // Check if MIDI has reached the end and should loop
        if (midiIsPlaying && midiMessageCurrent == nullptr && midiMessageHead != nullptr && !midiIsFading)
        {
            // Loop back to the beginning
            midiMessageCurrent = midiMessageHead;
            midiTime = 0.0;
            if (soundFont != nullptr)
            {
                tsf_note_off_all(soundFont);
            }
        }

        SDL_UnlockMutex(audioMutex);
    }

    void AudioManager::SwitchToPendingMIDI()
    {
        // This is called from RenderAudio with mutex already locked

        // Free old MIDI messages
        if (midiMessageHead != nullptr)
        {
            tml_free(midiMessageHead);
            midiMessageHead = nullptr;
            midiMessageCurrent = nullptr;
        }

        // Parse and load the pending MIDI
        if (!pendingMidiData.empty())
        {
            tml_message* newMidiMessages = tml_load_memory(pendingMidiData.data(), static_cast<int>(pendingMidiData.size()));
            if (newMidiMessages != nullptr)
            {
                midiMessageHead = newMidiMessages;
                midiMessageCurrent = newMidiMessages;
                midiTime = 0.0;
                midiIsPlaying = true;

                // Reset TinySoundFont for new playback
                if (soundFont != nullptr)
                {
                    tsf_reset(soundFont);
                    tsf_channel_set_bank_preset(soundFont, 9, 128, 0);
                }
            }
            else
            {
                midiIsPlaying = false;
            }
        }
        else
        {
            midiIsPlaying = false;
        }

        // Clear pending state and release memory
        std::vector<int8_t>().swap(pendingMidiData);
        hasPendingMidi = false;
        midiIsFading = false;
        fadeStep = 0;
    }

    void AudioManager::ProcessMIDIMessages(double targetTime)
    {
        while (midiMessageCurrent != nullptr && midiMessageCurrent->time <= targetTime)
        {
            switch (midiMessageCurrent->type)
            {
                case TML_PROGRAM_CHANGE:
                    // Special handling for channel 9 (drums) - use bank 128
                    tsf_channel_set_presetnumber(soundFont, midiMessageCurrent->channel,
                        midiMessageCurrent->program, (midiMessageCurrent->channel == 9));
                    break;

                case TML_NOTE_ON:
                    tsf_channel_note_on(soundFont, midiMessageCurrent->channel,
                        midiMessageCurrent->key, midiMessageCurrent->velocity / 127.0f);
                    break;

                case TML_NOTE_OFF:
                    tsf_channel_note_off(soundFont, midiMessageCurrent->channel,
                        midiMessageCurrent->key);
                    break;

                case TML_PITCH_BEND:
                    tsf_channel_set_pitchwheel(soundFont, midiMessageCurrent->channel,
                        midiMessageCurrent->pitch_bend);
                    break;

                case TML_CONTROL_CHANGE:
                    tsf_channel_midi_control(soundFont, midiMessageCurrent->channel,
                        midiMessageCurrent->control, midiMessageCurrent->control_value);
                    break;

                default:
                    break;
            }

            midiMessageCurrent = midiMessageCurrent->next;
        }
    }

    bool AudioManager::PlaySound(int32_t waveID, int32_t loopCount)
    {
        if (!initialized)
        {
            return false;
        }

        // Check if we can replay the same sound
        if (waveID == lastWaveID && loopCount == lastWaveLoops)
        {
            return WaveReplay();
        }

        // Generate the WAV data from SoundTrack
        // The buffer contains a WAV file with 44-byte header, audio data starts at byte 44
        Buffer* buffer = SoundTrack::Generate(loopCount, waveID);
        if (buffer == nullptr)
        {
            return false;
        }

        // Audio data length is total position minus the 44-byte WAV header
        static constexpr int32_t WAV_HEADER_SIZE = 44;
        int32_t audioLength = buffer->position - WAV_HEADER_SIZE;
        if (audioLength <= 0)
        {
            return false;
        }

        // Calculate timing (sample rate is 22050Hz, 8-bit = 1 byte per sample)
        uint64_t currentTime = SDL_GetTicks();
        uint64_t estimatedEndTime = lastWaveStartTime + (lastWaveLength / 22);

        if ((currentTime + (audioLength / 22)) > estimatedEndTime)
        {
            lastWaveLength = audioLength;
            lastWaveStartTime = currentTime;

            // Skip the 44-byte WAV header, pass only audio data
            if (WaveSave(buffer->data.data() + WAV_HEADER_SIZE, audioLength))
            {
                lastWaveID = waveID;
                lastWaveLoops = loopCount;
                return true;
            }
            return false;
        }

        // Timing check didn't pass, sound was not needed yet (not a failure)
        return true;
    }

    bool AudioManager::WaveReplay()
    {
        if (!initialized || currentWaveSamples.empty())
        {
            return false;
        }

        SDL_LockMutex(audioMutex);

        // Always restart from beginning for replay
        lastWaveStartTime = SDL_GetTicks();
        waveSamplePosition = 0;
        waveIsPlaying = true;

        SDL_UnlockMutex(audioMutex);
        return true;
    }

    bool AudioManager::WaveSave(int8_t* data, int32_t length)
    {
        if (!initialized)
        {
            return false;
        }

        SDL_LockMutex(audioMutex);

        // Convert 8-bit unsigned mono (22050 Hz) to float mono (44100 Hz)
        // The input is 8-bit unsigned, so 128 is silence
        currentWaveSamples.clear();
        currentWaveSamples.reserve(length * 2); // Upsample 22050 -> 44100

        for (int32_t i = 0; i < length; i++)
        {
            // Convert 8-bit unsigned to float (-1.0 to 1.0)
            // The data is stored as int8_t but represents unsigned 8-bit audio
            float sample = (static_cast<uint8_t>(data[i]) - 128) / 128.0f;

            // Duplicate sample for upsampling (simple nearest-neighbor)
            currentWaveSamples.push_back(sample);
            currentWaveSamples.push_back(sample);
        }

        waveSamplePosition = 0;
        waveIsPlaying = true;

        SDL_UnlockMutex(audioMutex);
        return true;
    }

    void AudioManager::PlayMIDI(std::vector<int8_t>& data, bool fade)
    {
        if (!initialized || data.empty())
        {
            return;
        }

        if (soundFont == nullptr)
        {
            return;
        }

        SDL_LockMutex(audioMutex);

        // If fade requested and currently playing, fade out old song first
        if (fade && midiIsPlaying)
        {
            // Store the new MIDI as pending
            pendingMidiData = data;
            hasPendingMidi = true;

            // Start fading
            midiIsFading = true;
            fadeStep = 0;
            fadeLastStepTime = SDL_GetTicks();

            SDL_UnlockMutex(audioMutex);
            return;
        }

        // No fade needed (or nothing playing) - play immediately

        // Free old MIDI messages
        if (midiMessageHead != nullptr)
        {
            tml_free(midiMessageHead);
            midiMessageHead = nullptr;
        }

        // Clear any pending fade state
        midiIsFading = false;
        hasPendingMidi = false;
        pendingMidiData.clear();
        fadeStep = 0;

        // Parse MIDI file with TML
        tml_message* newMidiMessages = tml_load_memory(data.data(), static_cast<int>(data.size()));
        if (newMidiMessages == nullptr)
        {
            SDL_UnlockMutex(audioMutex);
            return;
        }

        // Set new MIDI
        midiMessageHead = newMidiMessages;
        midiMessageCurrent = newMidiMessages;
        midiTime = 0.0;
        midiIsPlaying = true;

        // Reset TinySoundFont for new playback
        tsf_reset(soundFont);

        // Initialize percussion channel (channel 9/10 in MIDI uses bank 128)
        tsf_channel_set_bank_preset(soundFont, 9, 128, 0);

        SDL_UnlockMutex(audioMutex);
    }

    void AudioManager::StopMIDI()
    {
        if (!initialized)
        {
            return;
        }

        SDL_LockMutex(audioMutex);

        midiIsPlaying = false;
        midiIsFading = false;
        fadeStep = 0;
        hasPendingMidi = false;
        std::vector<int8_t>().swap(pendingMidiData);

        // Free MIDI messages
        if (midiMessageHead != nullptr)
        {
            tml_free(midiMessageHead);
            midiMessageHead = nullptr;
            midiMessageCurrent = nullptr;
        }

        midiTime = 0.0;

        // Stop all playing notes
        if (soundFont != nullptr)
        {
            tsf_reset(soundFont);
        }

        SDL_UnlockMutex(audioMutex);
    }

    void AudioManager::SetMIDIVolume(int32_t volume)
    {
        midiVolumeMillibels = volume;

        // Convert from millibels to linear gain
        // volume is in range -1200 to 0, where 0 is max
        if (volume <= -1200)
        {
            midiGain = 0.0f;
        }
        else if (volume >= 0)
        {
            midiGain = 1.0f;
        }
        else
        {
            // Convert millibels to linear: 10^(millibels/2000)
            midiGain = std::pow(10.0f, volume / 2000.0f);
        }
    }

    void AudioManager::SetSoundVolume(int32_t volume)
    {
        soundVolumeMillibels = volume;

        // Convert from millibels to linear gain
        if (volume <= -1200)
        {
            soundGain = 0.0f;
        }
        else if (volume >= 0)
        {
            soundGain = 1.0f;
        }
        else
        {
            soundGain = std::pow(10.0f, volume / 2000.0f);
        }
    }

    bool AudioManager::LoadSoundFont(const char* path)
    {
        SDL_LockMutex(audioMutex);

        // Free existing SoundFont if any
        if (soundFont != nullptr)
        {
            tsf_close(soundFont);
            soundFont = nullptr;
        }

        // Load SoundFont from file
        soundFont = tsf_load_filename(path);
        if (soundFont == nullptr)
        {
            SDL_UnlockMutex(audioMutex);
            return false;
        }

        // Set output mode to stereo interleaved at our sample rate
        // Use -10dB global gain to prevent clipping
        tsf_set_output(soundFont, TSF_STEREO_INTERLEAVED, SAMPLE_RATE, -10.0f);

        // Pre-allocate voices to avoid allocation during playback
        tsf_set_max_voices(soundFont, 256);

        SDL_UnlockMutex(audioMutex);
        return true;
    }
}
