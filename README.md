# C++ 317-Client

A cross-platform C++ game client for RuneScape revision 317, built with SDL3.

## Notable Features

- Fully functional 317 client using the [SDL3 stack](https://wiki.libsdl.org/SDL3/FrontPage)
- Cross-platform
- Web client support using [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) with websockets 
- Client resizing with GPU accelerated upscaling
- Follows the original client's design and structure
- Audio system with sound effects and MIDI music playback via soundfonts

## Missing Features

- Chat filter
- Anti-cheats

## Requirements

- CMake 3.16+
- C++26 compatible compiler
- For web builds: [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) and server that supports websockets (for reference: [reinismu/apollo](https://github.com/reinismu/apollo))

## Cloning and Building

If you are a complete beginner you may want to check out the SDL 3 sample project first: https://github.com/Ravbug/sdl3-sample

```bash
# You need to clone with submodules, otherwise SDL will not download.
git clone [repo url] --depth=1 --recurse-submodules
```

**Place your cache files in the `./cache/` directory. When you build and run the client, CMake will automatically copy this directory to the output location, or you can simply use JAGGRAB.**

### Windows

```bash
cd config
config-win.bat
cmake --build ..\build\win --config Release
```

The executable will be in `build/win/Release/`.

### Web (Emscripten)

```bash
# Open emcmdprompt.bat from your emsdk installation, then:
cd config
config-web-win.bat
cd ..\build\web
ninja
emrun SDL-Client.html
```

See [config/README.md](config/README.md) for detailed build instructions.