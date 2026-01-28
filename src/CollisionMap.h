#pragma once
#include "PCH.h"

namespace SDL_Client
{
    class CollisionMap {
    public:
        // ---- Flag definitions ----
        static constexpr int32_t FLAG_BLOCK_ENTITY_NW       = 0x1;
        static constexpr int32_t FLAG_BLOCK_ENTITY_N        = 0x2;
        static constexpr int32_t FLAG_BLOCK_ENTITY_NE       = 0x4;
        static constexpr int32_t FLAG_BLOCK_ENTITY_E        = 0x8;
        static constexpr int32_t FLAG_BLOCK_ENTITY_SE       = 0x10;
        static constexpr int32_t FLAG_BLOCK_ENTITY_S        = 0x20;
        static constexpr int32_t FLAG_BLOCK_ENTITY_SW       = 0x40;
        static constexpr int32_t FLAG_BLOCK_ENTITY_W        = 0x80;
        static constexpr int32_t FLAG_BLOCK_ENTITY          = 0x100;

        static constexpr int32_t FLAG_BLOCK_PROJECTILE_NW   = 0x200;
        static constexpr int32_t FLAG_BLOCK_PROJECTILE_N    = 0x400;
        static constexpr int32_t FLAG_BLOCK_PROJECTILE_NE   = 0x800;
        static constexpr int32_t FLAG_BLOCK_PROJECTILE_E    = 0x1000;
        static constexpr int32_t FLAG_BLOCK_PROJECTILE_SE   = 0x2000;
        static constexpr int32_t FLAG_BLOCK_PROJECTILE_S    = 0x4000;
        static constexpr int32_t FLAG_BLOCK_PROJECTILE_SW   = 0x8000;
        static constexpr int32_t FLAG_BLOCK_PROJECTILE_W    = 0x10000;
        static constexpr int32_t FLAG_BLOCK_PROJECTILE      = 0x20000;

        static constexpr int32_t FLAG_UNINITIALIZED         = 0x1000000;
        static constexpr int32_t FLAG_CLOSED                = 0xFFFFFF;

        CollisionMap(int32_t sizeX, int32_t sizeZ);

        void Reset();
        void AddWall(int32_t x, int32_t z, int32_t type, int32_t rotation, bool projectiles);
        void Add(bool blocksProjectiles, int32_t sizeX, int32_t sizeZ, int32_t x, int32_t z, int32_t rotation);

        void AddSolid(int32_t x, int32_t z);
        void Add(int32_t x, int32_t z, int32_t flags);

        void Remove(int32_t x, int32_t z, int32_t rotation, int32_t type, bool projectiles);
        void Remove(int32_t rotation, int32_t sizeX, int32_t x0, int32_t z0, int32_t sizeZ, bool projectiles);
        void Remove(int32_t x, int32_t z, int32_t flags);
        void RemoveSolid(int32_t x, int32_t z);

        bool ReachedDestination(int32_t sx, int32_t sz, int32_t dx, int32_t dz, int32_t rotation, int32_t type);
        bool ReachedWall(int32_t sx, int32_t sz, int32_t dx, int32_t dz, int32_t type, int32_t rotation);
        bool ReachedLoc(int32_t srcX, int32_t srcZ, int32_t dstX, int32_t dstZ,
                        int32_t dstSizeX, int32_t dstSizeZ, int32_t interactionSides);

        int32_t sizeX = 0;
        int32_t sizeZ = 0;
        Array2DIndexed<int32_t> flags;
    };
}