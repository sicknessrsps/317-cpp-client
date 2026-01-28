#pragma once

#include "PCH.h"
#include "Buffer.h"

// Forward declarations for TinySoundFont
struct tsf;
struct tml_message;

namespace SDL_Client
{
    class AudioManager
    {
    public:
        AudioManager() = default;
        ~AudioManager();

        bool Init();
        void Shutdown();

        // Sound effect playback (WAV) - returns true on success
        bool PlaySound(int32_t waveID, int32_t loopCount);
        bool WaveReplay();
        bool WaveSave(int8_t* data, int32_t length);

        // MIDI/Music playback
        void PlayMIDI(std::vector<int8_t>& data, bool fade);
        void StopMIDI();
        void SetMIDIVolume(int32_t volume);

        // Volume control
        void SetSoundVolume(int32_t volume);

        // SoundFont management
        bool LoadSoundFont(const char* path);

    private:
        // SDL3 audio callback (static to match SDL callback signature)
        static void SDLCALL AudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);

        // Internal audio rendering
        void RenderAudio(float* buffer, int samples);
        void ProcessMIDIMessages(double targetTime);
        void SwitchToPendingMIDI();

    private:
        bool initialized = false;

        // SDL3 audio device and stream
        SDL_AudioDeviceID audioDevice = 0;
        SDL_AudioStream* audioStream = nullptr;

        // Audio format settings
        static constexpr int SAMPLE_RATE = 44100;
        static constexpr int CHANNELS = 2;

        // Current sound effect state (matches Java's lastWave* fields)
        std::vector<float> currentWaveSamples;
        size_t waveSamplePosition = 0;
        bool waveIsPlaying = false;
        int32_t lastWaveID = -1;
        int32_t lastWaveLoops = -1;
        uint64_t lastWaveStartTime = 0;
        int32_t lastWaveLength = 0;

        // TinySoundFont state
        tsf* soundFont = nullptr;
        tml_message* midiMessageHead = nullptr;    // Start of MIDI message list (for freeing/looping)
        tml_message* midiMessageCurrent = nullptr; // Current position in playback
        double midiTime = 0.0;                     // Current playback time in milliseconds
        bool midiIsPlaying = false;

        // SDL mutex for thread safety
        SDL_Mutex* audioMutex = nullptr;

        // MIDI fade state - fade OUT old song before switching to new
        // Based on original JS: 36 steps * 200ms = 7200ms fade, volume -= 100 millibels per step
        bool midiIsFading = false;
        int32_t fadeStep = 0;                      // Current fade step (0-36)
        uint64_t fadeLastStepTime = 0;             // Time of last fade step
        static constexpr int32_t FADE_STEPS = 36;  // Total steps to fade
        static constexpr int32_t FADE_STEP_MS = 200; // Time per step in ms

        // Pending MIDI to play after fade completes
        std::vector<int8_t> pendingMidiData;
        bool hasPendingMidi = false;

        // Volume in millibels (-1200 to 0, where 0 is max)
        int32_t midiVolumeMillibels = 0;
        int32_t soundVolumeMillibels = 0;

        // Computed gain (0.0 to 1.0)
        float soundGain = 1.0f;
        float midiGain = 1.0f;

        // Pre-allocated render buffer to avoid heap allocation in audio callback
        std::vector<float> renderBuffer;
    };
}
