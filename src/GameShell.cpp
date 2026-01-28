#include "GameShell.h"

#include "Helvetica_bold.h"

namespace SDL_Client {
    GameShell::GameShell(const GameSpecification& specification)
        : screenWidth(specification.width), screenHeight(specification.height),
          gpuUpscaling(specification.gpuUpscaling), linearFiltering(specification.linearFiltering),
          specification(specification) {}

    SDL_AppResult GameShell::Init()
    {
        SDL_SetAppMetadata(specification.name.c_str(), specification.version.c_str(), specification.identifier.c_str());

        if (!SDL_Init(SDL_INIT_VIDEO)) {
            LOG_ERROR("Couldn't initialize SDL: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        // init TTF
        if (!TTF_Init()) {
            LOG_ERROR("Couldn't initialize TTF: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        SDL_IOStream* pFontMem = SDL_IOFromConstMem(Helvetica_bold, sizeof(Helvetica_bold));
        helveticaFont = TTF_OpenFontIO(pFontMem, true, 13);
        if (!helveticaFont)
        {
            LOG_ERROR("Couldn't load font: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        // Create window - resizable when GPU upscaling is enabled
        SDL_WindowFlags windowFlags = gpuUpscaling ? SDL_WINDOW_RESIZABLE : 0;
        window = SDL_CreateWindow(specification.title.c_str(), screenWidth, screenHeight, windowFlags);
        if (!window) {
            LOG_ERROR("Couldn't create window: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        // Set minimum window size to prevent scaling below base resolution
        SDL_SetWindowMinimumSize(window, screenWidth, screenHeight);

        // Set up GPU upscaling if enabled
        if (gpuUpscaling) {
            renderer = SDL_CreateRenderer(window, nullptr);
            if (!renderer) {
                LOG_WARN("Couldn't create renderer: %s - falling back to software rendering", SDL_GetError());
                gpuUpscaling = false;
            } else {
                SDL_SetRenderLogicalPresentation(renderer, screenWidth, screenHeight,
                    SDL_LOGICAL_PRESENTATION_LETTERBOX);

                gpuTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                    SDL_TEXTUREACCESS_STREAMING, screenWidth, screenHeight);
                if (!gpuTexture) {
                    LOG_WARN("Couldn't create GPU texture: %s - falling back to software rendering", SDL_GetError());
                    SDL_DestroyRenderer(renderer);
                    renderer = nullptr;
                    gpuUpscaling = false;
                } else {
                    SDL_SetTextureScaleMode(gpuTexture, linearFiltering ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                    LOG_INFO("GPU upscaling enabled with resizable window (%s filtering)", linearFiltering ? "linear" : "nearest");
                }
            }
        }

         if (!NET_Init()) {
             LOG_ERROR("NET_Init() failed: %s", SDL_GetError());
             return SDL_APP_FAILURE;
         }

        // Create software render surface at base resolution
        drawSurface = SDL_CreateSurface(screenWidth, screenHeight, SDL_PIXELFORMAT_ARGB8888);
        SDL_SetSurfaceBlendMode(drawSurface, SDL_BLENDMODE_NONE);

        SDL_StartTextInput(window);

        //Signlink::StartPriv();
        Signlink::Run();

        SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "0");

        // Initialize loading mutex
        loadingMessageMutex = SDL_CreateMutex();
        if (!loadingMessageMutex) {
            LOG_ERROR("Failed to create loading mutex: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        // Initialize loading progress
        SDL_SetAtomicInt(&loadingPercent, 0);
        SDL_LockMutex(loadingMessageMutex);
        loadingMessage = "Loading...";
        SDL_UnlockMutex(loadingMessageMutex);

        // Start loading in a background thread
        SDL_SetAtomicInt(&loadingState, static_cast<int>(LoadingState::Loading));
        loadThread = SDL_CreateThread(LoadThreadFunc, "LoadThread", this);
        if (!loadThread) {
            LOG_ERROR("Failed to create loading thread: %s", SDL_GetError());
            SDL_SetAtomicInt(&loadingState, static_cast<int>(LoadingState::Error));
            return SDL_APP_FAILURE;
        }

        for (int k1 = 0; k1 < 10; k1++) {
            otim[k1] = SDL_GetTicks();
        }

        LOG_INFO("Application initialized!");
        return SDL_APP_CONTINUE;
    }

    int32_t GameShell::LoadThreadFunc(void* ptr) {
        auto* shell = static_cast<GameShell*>(ptr);
        shell->Load();
        SDL_SetAtomicInt(&shell->loadingState, static_cast<int>(LoadingState::Complete));
        return 0;
    }

    SDL_AppResult GameShell::Run()
    {
        // While loading, just draw progress and return
        if (SDL_GetAtomicInt(&loadingState) == static_cast<int>(LoadingState::Loading)) {
            DrawLoadingProgress();
            SDL_Delay(16); // ~60 fps during loading
            return SDL_APP_CONTINUE;
        }

        // Check for loading completion - wait for thread to finish
        if (loadThread) {
            SDL_WaitThread(loadThread, nullptr);
            loadThread = nullptr;
        }

        uint32_t lastRatio = ratio;
        uint32_t lastDelta = delta;

        ratio = 300;
        delta = 1;

        Uint64 ntime = SDL_GetTicks();
        if (otim[opos] == 0) {
            ratio = lastRatio;
            delta = lastDelta;
        } else if (ntime > otim[opos]) {
            ratio = (2560 * deltime) / (ntime - otim[opos]);
        }

        if (ratio < 25) {
            ratio = 25;
        }

        if (ratio > 256) {
            ratio = 256;
            delta = deltime - ((ntime - otim[opos]) / 10);
        }

        if (delta > deltime) {
            delta = deltime;
        }

        otim[opos] = ntime;
        opos = (opos + 1) % 10;

        if (delta > 1) {
            for (uint32_t k2 = 0; k2 < 10; k2++) {
                if (otim[k2] != 0L) {
                    otim[k2] += delta;
                }
            }
        }

        if (delta < mindel) {
            delta = mindel;
        }

        SDL_Delay(delta);

        Uint64 time = SDL_GetTicksNS();
        for (; count < 256; count += ratio) {
            mouseClickButton = lastMouseClickButton;
            mouseClickX = lastMouseClickX;
            mouseClickY = lastMouseClickY;
            mouseClickTime = lastMouseClickTime;
            lastMouseClickButton = 0;
            Update();
            keyQueueReadPos = keyQueueWritePos;
        }

        count &= 0xff;

        if (deltime > 0) {
            fps = (1000 * ratio) / (deltime * 256);
        }

        Draw();

        frameTime[fpos] = static_cast<double_t>(SDL_GetTicksNS() - time) / 1000000.0;
        fpos = (fpos + 1) % frameTime.size();

        if (debug) {
            LOG_INFO("ntime:%d", ntime);
            for (int i = 0; i < 10; i++) {
                int o = ((opos - i - 1) + 20) % 10;
                LOG_INFO("otim%d:%d", o, otim[o]);
            }
            LOG_INFO("fps:%d ratio:%d count:%d", fps, ratio, count);
            LOG_INFO("del:%d deltime:%d mindel:%d", delta, deltime, mindel);
            LOG_INFO("opos:%d", opos);
            debug = false;
        }

        return SDL_APP_CONTINUE;
    }

    SDL_AppResult GameShell::PollEvent(const SDL_Event* event)
    {
        if (event->type == SDL_EVENT_QUIT) {
            return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
        }
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            idleCycles = 0;
            float mx = event->button.x;
            float my = event->button.y;
            // Transform coordinates when GPU upscaling is enabled
            if (gpuUpscaling && renderer) {
                SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
            }
            lastMouseClickX = mx;
            lastMouseClickY = my;
            lastMouseClickTime = SDL_GetTicks();
            if (event->button.button == SDL_BUTTON_LEFT) {
                lastMouseClickButton = 1;
                mouseButton = 1;
            }
            if (event->button.button == SDL_BUTTON_RIGHT) {
                lastMouseClickButton = 2;
                mouseButton = 2;
            }
        }
        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            idleCycles = 0;
            mouseButton = 0;
        }
        if (event->type == SDL_EVENT_MOUSE_MOTION) {
            idleCycles = 0;
            float mx = event->motion.x;
            float my = event->motion.y;
            // Transform coordinates when GPU upscaling is enabled
            if (gpuUpscaling && renderer) {
                SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
            }
            mouseX = mx;
            mouseY = my;
        }
        if (event->type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
            idleCycles = 0;
            mouseX = -1;
            mouseY = -1;
        }
        if (event->type == SDL_EVENT_KEY_DOWN) {
            idleCycles = 0;
            SDL_Keycode code = event->key.key;
            int32_t value = 0;

            // For letter keys, don't set value here - wait for TEXT_INPUT
            // Only handle non-printable special keys here

            if (code == SDLK_LEFT) {
                value = 1;
            } else if (code == SDLK_RIGHT) {
                value = 2;
            } else if (code == SDLK_UP) {
                value = 3;
            } else if (code == SDLK_DOWN) {
                value = 4;
            } else if (code == SDLK_LCTRL || code == SDLK_RCTRL) {
                value = 5;
            } else if (code == SDLK_BACKSPACE || code == SDLK_DELETE) {
                value = 8;
            } else if (code == SDLK_TAB) {
                value = 9;
            } else if (code == SDLK_RETURN) {
                value = 10;
            } else if (code >= SDLK_F1 && code <= SDLK_F12) {
                value = 1008 + (code - SDLK_F1);
            } else if (code == SDLK_HOME) {
                value = 1000;
            } else if (code == SDLK_END) {
                value = 1001;
            } else if (code == SDLK_PAGEUP) {
                value = 1002;
            } else if (code == SDLK_PAGEDOWN) {
                value = 1003;
            }

            if (value > 0 && value < 128) {
                actionKey[value] = 1;
            }

            if (value > 4) {
                keyQueue[keyQueueWritePos] = value;
                keyQueueWritePos = (keyQueueWritePos + 1) & 0x7f;
            }
        } else if (event->type == SDL_EVENT_TEXT_INPUT) {
            // Handle actual text input (includes uppercase/lowercase)
            idleCycles = 0;
            const char* text = event->text.text;

            if (text[0] != '\0') {
                auto value = static_cast<int32_t>(text[0]);

                if (value >= 32 && value < 128) {
                    actionKey[value] = 1;
                    keyQueue[keyQueueWritePos] = value;
                    keyQueueWritePos = (keyQueueWritePos + 1) & 0x7f;
                }
            }
        }
        if (event->type == SDL_EVENT_KEY_UP) {
            idleCycles = 0;
            SDL_Keycode code = event->key.key;
            int32_t action = event->key.key; //modify

            if (action < 30) { //ACTION
                action = 0;
            }

            if (code == SDLK_LEFT) {
                action = 1;
            } else if (code == SDLK_RIGHT) {
                action = 2;
            } else if (code == SDLK_UP) {
                action = 3;
            } else if (code == SDLK_DOWN) {
                action = 4;
            } else if (code == SDLK_LCTRL || code == SDLK_RCTRL) {
                action = 5;
            } else if (code == SDLK_BACKSPACE) {
                action = 8;
            } else if (code == SDLK_DELETE) {
                action = 8;
            } else if (code == SDLK_TAB) {
                action = 9;
            } else if (code == SDLK_RETURN || code == SDLK_KP_ENTER) {
                action = 10;
            }

            if ((action > 0) && (action < 128)) {
                actionKey[action] = 0;
            }
        }
        if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            focused = false;
            for (int & i : actionKey) {
                i = 0;
            }
        }
        if (event->type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
            focused = true;
        }
        return SDL_APP_CONTINUE;
    }

    void GameShell::Shutdown()
    {
        // Wait for loading thread to finish if still running
        if (loadThread) {
            SDL_WaitThread(loadThread, nullptr);
            loadThread = nullptr;
        }

        // Clean up loading mutex
        if (loadingMessageMutex) {
            SDL_DestroyMutex(loadingMessageMutex);
            loadingMessageMutex = nullptr;
        }

        Unload();

        // Clean up SDL resources in reverse order of creation
        if (helveticaFont) {
            TTF_CloseFont(helveticaFont);
            helveticaFont = nullptr;
        }

        if (drawSurface) {
            SDL_DestroySurface(drawSurface);
            drawSurface = nullptr;
        }

        // Clean up GPU upscaling resources
        if (gpuTexture) {
            SDL_DestroyTexture(gpuTexture);
            gpuTexture = nullptr;
        }

        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }

        if (window) {
            SDL_StopTextInput(window);
            SDL_DestroyWindow(window);
            window = nullptr;
        }

        // Quit extension libraries (TTF, NET) - these are separate from SDL core
        NET_Quit();
        TTF_Quit();

        // Note: Do NOT call SDL_Quit() when using SDL_MAIN_USE_CALLBACKS.
        // SDL's callback system manages SDL_Quit internally after SDL_AppQuit returns.

        LOG_INFO("Application shutdown!");
    }

    void GameShell::PresentFrame()
    {
        if (!window || !drawSurface || !drawSurface->pixels) {
            return;
        }

        if (renderer && gpuTexture) {
            // GPU rendering path - upload software-rendered surface to GPU texture
            SDL_UpdateTexture(gpuTexture, nullptr, drawSurface->pixels, drawSurface->pitch);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, gpuTexture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        } else {
            // Software rendering path - blit to window surface
            SDL_Surface* windowSurface = SDL_GetWindowSurface(window);
            if (windowSurface) {
                SDL_BlitSurface(drawSurface, nullptr, windowSurface, nullptr);
                SDL_UpdateWindowSurface(window);
            }
        }
    }

    void GameShell::SetFramerate(int32_t fps)
    {
        deltime = 1000 / fps;
    }

    void GameShell::DrawProgress(int32_t percent, const std::string &message)
    {
        // Update shared progress state (thread-safe)
        SDL_SetAtomicInt(&loadingPercent, percent);
        SDL_LockMutex(loadingMessageMutex);
        loadingMessage = message;
        SDL_UnlockMutex(loadingMessageMutex);
    }

    void GameShell::DrawLoadingProgress()
    {
        // Read current progress (thread-safe)
        int32_t percent = SDL_GetAtomicInt(&loadingPercent);
        std::string message;
        SDL_LockMutex(loadingMessageMutex);
        message = loadingMessage;
        SDL_UnlockMutex(loadingMessageMutex);

        const int32_t x = screenWidth;
        const int32_t y = screenHeight;
        const int32_t midY = (y / 2) - 18;

        Draw2D::Bind(drawSurface);
        Draw2D::Clear(); // clear background

        Draw2D::DrawRect((x / 2) - 152, midY, 304, 34, 0xFF8c1111);
        Draw2D::DrawRect((x / 2) - 151, midY + 1, 302, 32, 0xFF000000);
        Draw2D::FillRect((x / 2) - 150, midY + 2, percent * 3, 30, 0xFF8c1111);

        // empty part
        Draw2D::FillRect(((x / 2) - 150) + (percent * 3), midY + 2,
                         300 - (percent * 3), 30, 0xFF000000);

        // render message centered
        SDL_Surface* surface = TTF_RenderText_Solid(helveticaFont, message.data(), message.length(), { 255,255,255 });
        if (surface) {
            const int textX = (x - surface->w) / 2;
            const int textY = midY + (34 - surface->h) / 2;
            const SDL_Rect dst = { textX, textY, surface->w, surface->h };
            SDL_BlitSurface(surface, nullptr, drawSurface, &dst);
            SDL_DestroySurface(surface);
        }

        PresentFrame();
    }

    int32_t GameShell::PollKey() {
        int32_t key = -1;
        if (keyQueueWritePos != keyQueueReadPos) {
            key = keyQueue[keyQueueReadPos];
            keyQueueReadPos = (keyQueueReadPos + 1) & 0x7f;
        }
        return key;
    }
}
