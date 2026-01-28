#pragma once
#define NOMINMAX  // Prevent Windows. h from defining min/max macros
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_net/SDL_net.h>
#include <algorithm>
#include <functional>
#include <random>
#include <cmath>
#include <deque>
#include <list>
#include <chrono>
#include <cstdint>
#include <array>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <fstream>
#include <mutex>
#include <cassert>
#include <span>
#include <zlib.h>

#include "ArrayND.h"
#include "LRUMap.h"
#include "DoublyLinkedList.h"


#define LOG_INFO(...)  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOG_WARN(...)  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOG_ERROR(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
//#define LOG_DEBUG(...) SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

#define ASSERT(cond) SDL_assert(cond)