#include "MapUtil.h"

namespace SDL_Client
{
    int32_t MapUtil::RotateX(int32_t x, int32_t z, int32_t rotation) {
        rotation &= 3;
        if (rotation == 0) {
            return x;
        }
        if (rotation == 1) {
            return z;
        }
        if (rotation == 2) {
            return 7 - x;
        } else {
            return 7 - z;
        }
    }

    int32_t MapUtil::RotateZ(int32_t x, int32_t z, int32_t rotation) {
        rotation &= 3;
        if (rotation == 0) {
            return z;
        }
        if (rotation == 1) {
            return 7 - x;
        }
        if (rotation == 2) {
            return 7 - z;
        } else {
            return x;
        }
    }

    int32_t MapUtil::RotateLocX(int32_t x, int32_t z, int32_t sizeX, int32_t sizeZ, int32_t rotation) {
        rotation &= 3;
        if (rotation == 0) {
            return x;
        }
        if (rotation == 1) {
            return z;
        }
        if (rotation == 2) {
            return 7 - x - (sizeX - 1);
        } else {
            return 7 - z - (sizeZ - 1);
        }
    }

    int32_t MapUtil::RotateLocZ(int32_t x, int32_t z, int32_t sizeX, int32_t sizeZ, int32_t rotation) {
        rotation &= 3;
        if (rotation == 0) {
            return z;
        }
        if (rotation == 1) {
            return 7 - x - (sizeX - 1);
        }
        if (rotation == 2) {
            return 7 - z - (sizeZ - 1);
        } else {
            return x;
        }
    }
}