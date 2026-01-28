# Build Configuration Scripts

This directory contains build configuration scripts for different platforms.

## Windows Native Build

1. Open a command prompt in this directory
2. Run `config-win.bat`
3. Build: `cmake --build ..\build\win --config Release`
4. Run the executable from `build\win\Release\`

## Emscripten (Web) Build

### Prerequisites
- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) installed and activated

### Steps
1. Open `emcmdprompt.bat` from your emsdk installation
2. Navigate to this directory
3. Run `config-web-win.bat`
4. Build: `cd ..\build\web && ninja` (or `mingw32-make`)
5. Run: `emrun SDL-Client.html`

### Serving Locally
The web build requires a server with proper CORS headers for SharedArrayBuffer (required for pthreads). Use `emrun` or configure your server with:
```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```