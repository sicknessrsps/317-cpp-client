#pragma once

#include "PCH.h"

namespace SDL_Client
{
    class MapUtil {
    public:
        static int32_t RotateX(int32_t x, int32_t z, int32_t rotation);
        static int32_t RotateZ(int32_t x, int32_t z, int32_t rotation);
        static int32_t RotateLocX(int32_t x, int32_t z, int32_t sizeX, int32_t sizeZ, int32_t rotation);
        static int32_t RotateLocZ(int32_t x, int32_t z, int32_t sizeX, int32_t sizeZ, int32_t rotation);
    };
}