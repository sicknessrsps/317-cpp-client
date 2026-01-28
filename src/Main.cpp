#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include "Game.h"
#include "GameShell.h"

SDL_Client::Game* game = nullptr;

/* This function runs once at startup.
 */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    const SDL_Client::GameSpecification spec
    {
        "SDL Client",
        "C++ SDL Client #317",
        "1.0",
        "com.example.sdl-client",
        765,
        503,
        "127.0.0.1",
        43594,
        false,  // enableRSA
        false,  // lowmem
        true,   // gpuUpscaling - enables resizable window with GPU texture rendering
        true    // linearFiltering - true = smooth scaling, false = crisp/pixelated
    };
    game = new SDL_Client::Game(spec);
    return game->Init();  /* carry on with the program! */
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    return game->PollEvent(event);  /* carry on with the program! */
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    return game->Run();  /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    game->Shutdown();
    delete game;
}