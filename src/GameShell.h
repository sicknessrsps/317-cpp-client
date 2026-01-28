#pragma once
#include "PCH.h"
#include "Draw2D.h"
#include "Signlink.h"

namespace SDL_Client {

    enum class LoadingState {
        NotStarted,
        Loading,
        Complete,
        Error
    };

    struct GameSpecification
    {
        std::string name = "SDL Client";
        std::string title = "SDL Client";
        std::string version = "1.0";
        std::string identifier = "com.example.sdl-client";
        int32_t width = 765;
        int32_t height = 503;
        std::string serverAddress = "127.0.0.1";
        int32_t serverPort = 43594;
        bool enableRSA = false;
        bool lowmem = false;
        bool gpuUpscaling = false;
        bool linearFiltering = false;  // true = smooth scaling, false = crisp/pixelated
    };

    class GameShell {

    public:
        explicit GameShell(const GameSpecification& specification);
        virtual ~GameShell() = default;

        SDL_AppResult Init();
        SDL_AppResult Run();
        SDL_AppResult PollEvent(const SDL_Event *event);

        void Shutdown();

        void SetFramerate(int32_t fps);

        virtual void Update() = 0;
        virtual void Draw() = 0;
        virtual void Load() = 0;
        virtual void Unload() = 0;
        virtual void Refresh() = 0;

    protected:
        virtual void DrawProgress(int32_t percent, const std::string& message) = 0;
        virtual void DrawLoadingProgress();
        int32_t PollKey();
        static int32_t LoadThreadFunc(void* ptr);
        void PresentFrame();
        int32_t screenWidth, screenHeight;
        SDL_Surface* drawSurface = nullptr;
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* gpuTexture = nullptr;
        bool gpuUpscaling = false;
        bool linearFiltering = false;
        TTF_Font* helveticaFont = nullptr;

        bool debug = false;

        uint32_t mouseClickButton = 0;
        float mouseClickX = 0;
        float mouseClickY = 0;
        float mouseX = 0, mouseY = 0;
        uint32_t idleCycles = 0;

        int32_t actionKey[128]{};
        uint32_t fps = 0;
        std::array<double_t, 100> frameTime{};
        uint32_t mouseButton = 0;
    private:
        uint32_t opos = 0;
        uint32_t fpos = 0;
        uint32_t ratio = 256;
        uint32_t delta = 1;
        uint32_t count = 0;
        uint32_t deltime = 20;
        uint32_t mindel = 1;
        std::array<uint64_t, 10> otim{};
        GameSpecification specification;

        float lastMouseClickX = 0;
        float lastMouseClickY = 0;
        uint32_t lastMouseClickTime = 0;
        uint32_t lastMouseClickButton = 0;
        uint32_t mouseClickTime = 0;

        int32_t keyQueue[128]{};
        int32_t keyQueueReadPos = 0;
        int32_t keyQueueWritePos = 0;
        bool focused = true;

        // Loading thread state
        SDL_Thread* loadThread = nullptr;
        SDL_AtomicInt loadingState{static_cast<int>(LoadingState::NotStarted)};
        SDL_AtomicInt loadingPercent{0};
        std::string loadingMessage;
        SDL_Mutex* loadingMessageMutex = nullptr;
    };
}