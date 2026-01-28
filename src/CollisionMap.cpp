#include "CollisionMap.h"

namespace SDL_Client
{
    CollisionMap::CollisionMap(int32_t sizeX, int32_t sizeZ)
        : sizeX(sizeX), sizeZ(sizeZ), flags(sizeX, sizeZ)
    {
        Reset();
    }

    void CollisionMap::Reset()
    {
        for (int32_t x = 0; x < sizeX; x++) {
            for (int32_t z = 0; z < sizeZ; z++) {
                if (x == 0 || z == 0 || x == sizeX - 1 || z == sizeZ - 1)
                    flags[x][z] = FLAG_CLOSED;
                else
                    flags[x][z] = FLAG_UNINITIALIZED;
            }
        }
    }

    void CollisionMap::AddWall(int32_t x, int32_t z, int32_t type, int32_t rotation, bool projectiles)
    {
        if (type == 0) {
            if (rotation == 0) {
                Add(x, z, FLAG_BLOCK_ENTITY_W);
                Add(x - 1, z, FLAG_BLOCK_ENTITY_E);
            } else if (rotation == 1) {
                Add(x, z, FLAG_BLOCK_ENTITY_N);
                Add(x, z + 1, FLAG_BLOCK_ENTITY_S);
            } else if (rotation == 2) {
                Add(x, z, FLAG_BLOCK_ENTITY_E);
                Add(x + 1, z, FLAG_BLOCK_ENTITY_W);
            } else if (rotation == 3) {
                Add(x, z, FLAG_BLOCK_ENTITY_S);
                Add(x, z - 1, FLAG_BLOCK_ENTITY_N);
            }
        } else if ((type == 1) || (type == 3)) {
            if (rotation == 0) {
                Add(x, z, FLAG_BLOCK_ENTITY_NW);
                Add(x - 1, z + 1, FLAG_BLOCK_ENTITY_SE);
            } else if (rotation == 1) {
                Add(x, z, FLAG_BLOCK_ENTITY_NE);
                Add(x + 1, z + 1, FLAG_BLOCK_ENTITY_SW);
            } else if (rotation == 2) {
                Add(x, z, FLAG_BLOCK_ENTITY_SE);
                Add(x + 1, z - 1, FLAG_BLOCK_ENTITY_NW);
            } else if (rotation == 3) {
                Add(x, z, FLAG_BLOCK_ENTITY_SW);
                Add(x - 1, z - 1, FLAG_BLOCK_ENTITY_NE);
            }
        } else if (type == 2) {
            if (rotation == 0) {
                Add(x, z, FLAG_BLOCK_ENTITY_W | FLAG_BLOCK_ENTITY_N);
                Add(x - 1, z, FLAG_BLOCK_ENTITY_E);
                Add(x, z + 1, FLAG_BLOCK_ENTITY_S);
            } else if (rotation == 1) {
                Add(x, z, FLAG_BLOCK_ENTITY_E | FLAG_BLOCK_ENTITY_N);
                Add(x, z + 1, FLAG_BLOCK_ENTITY_S);
                Add(x + 1, z, FLAG_BLOCK_ENTITY_W);
            } else if (rotation == 2) {
                Add(x, z, FLAG_BLOCK_ENTITY_E | FLAG_BLOCK_ENTITY_S);
                Add(x + 1, z, FLAG_BLOCK_ENTITY_W);
                Add(x, z - 1, FLAG_BLOCK_ENTITY_N);
            } else if (rotation == 3) {
                Add(x, z, FLAG_BLOCK_ENTITY_W | FLAG_BLOCK_ENTITY_S);
                Add(x, z - 1, FLAG_BLOCK_ENTITY_N);
                Add(x - 1, z, FLAG_BLOCK_ENTITY_E);
            }
        }

        if (projectiles) {
            if (type == 0) {
                if (rotation == 0) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_W);
                    Add(x - 1, z, FLAG_BLOCK_PROJECTILE_E);
                } else if (rotation == 1) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_N);
                    Add(x, z + 1, FLAG_BLOCK_PROJECTILE_S);
                } else if (rotation == 2) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_E);
                    Add(x + 1, z, FLAG_BLOCK_PROJECTILE_W);
                } else if (rotation == 3) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_S);
                    Add(x, z - 1, FLAG_BLOCK_PROJECTILE_N);
                }
            } else if ((type == 1) || (type == 3)) {
                if (rotation == 0) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_NW);
                    Add(x - 1, z + 1, FLAG_BLOCK_PROJECTILE_SE);
                } else if (rotation == 1) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_NE);
                    Add(x + 1, z + 1, FLAG_BLOCK_PROJECTILE_SW);
                } else if (rotation == 2) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_SE);
                    Add(x + 1, z - 1, FLAG_BLOCK_PROJECTILE_NW);
                } else if (rotation == 3) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_SW);
                    Add(x - 1, z - 1, FLAG_BLOCK_PROJECTILE_NE);
                }
            } else if (type == 2) {
                if (rotation == 0) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_W | FLAG_BLOCK_PROJECTILE_N);
                    Add(x - 1, z, FLAG_BLOCK_PROJECTILE_E);
                    Add(x, z + 1, FLAG_BLOCK_PROJECTILE_S);
                } else if (rotation == 1) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_E | FLAG_BLOCK_PROJECTILE_N);
                    Add(x, z + 1, FLAG_BLOCK_PROJECTILE_S);
                    Add(x + 1, z, FLAG_BLOCK_PROJECTILE_W);
                } else if (rotation == 2) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_E | FLAG_BLOCK_PROJECTILE_S);
                    Add(x + 1, z, FLAG_BLOCK_PROJECTILE_W);
                    Add(x, z - 1, FLAG_BLOCK_PROJECTILE_N);
                } else if (rotation == 3) {
                    Add(x, z, FLAG_BLOCK_PROJECTILE_W | FLAG_BLOCK_PROJECTILE_S);
                    Add(x, z - 1, FLAG_BLOCK_PROJECTILE_N);
                    Add(x - 1, z, FLAG_BLOCK_PROJECTILE_E);
                }
            }
        }
    }

    void CollisionMap::Add(bool blocksProjectiles, int32_t objSizeX, int32_t objSizeZ, int32_t x, int32_t z, int32_t rotation)
    {
        int32_t options = FLAG_BLOCK_ENTITY;

        if (blocksProjectiles) {
            options += FLAG_BLOCK_PROJECTILE;
        }

        if ((rotation == 1) || (rotation == 3)) {
            int32_t tmp = objSizeX;
            objSizeX = objSizeZ;
            objSizeZ = tmp;
        }

        for (int32_t tx = x; tx < (x + objSizeX); tx++) {
            if ((tx >= 0) && (tx < sizeX)) {
                for (int32_t tz = z; tz < (z + objSizeZ); tz++) {
                    if ((tz >= 0) && (tz < sizeZ)) {
                        Add(tx, tz, options);
                    }
                }
            }
        }
    }

    void CollisionMap::AddSolid(int32_t x, int32_t z)
    {
        flags[x][z] |= FLAG_UNINITIALIZED;
    }

    void CollisionMap::Add(int32_t x, int32_t z, int32_t options)
    {
        flags[x][z] |= options;
    }

    void CollisionMap::Remove(int32_t x, int32_t z, int32_t rotation, int32_t type, bool projectiles)
    {
        if (type == 0) {
            if (rotation == 0) {
                Remove(x, z, FLAG_BLOCK_ENTITY_W);
                Remove(x - 1, z, FLAG_BLOCK_ENTITY_E);
            } else if (rotation == 1) {
                Remove(x, z, FLAG_BLOCK_ENTITY_N);
                Remove(x, z + 1, FLAG_BLOCK_ENTITY_S);
            } else if (rotation == 2) {
                Remove(x, z, FLAG_BLOCK_ENTITY_E);
                Remove(x + 1, z, FLAG_BLOCK_ENTITY_W);
            } else if (rotation == 3) {
                Remove(x, z, FLAG_BLOCK_ENTITY_S);
                Remove(x, z - 1, FLAG_BLOCK_ENTITY_N);
            }
        } else if ((type == 1) || (type == 3)) {
            if (rotation == 0) {
                Remove(x, z, FLAG_BLOCK_ENTITY_NW);
                Remove(x - 1, z + 1, FLAG_BLOCK_ENTITY_SE);
            } else if (rotation == 1) {
                Remove(x, z, FLAG_BLOCK_ENTITY_NE);
                Remove(x + 1, z + 1, FLAG_BLOCK_ENTITY_SW);
            } else if (rotation == 2) {
                Remove(x, z, FLAG_BLOCK_ENTITY_SE);
                Remove(x + 1, z - 1, FLAG_BLOCK_ENTITY_NW);
            } else if (rotation == 3) {
                Remove(x, z, FLAG_BLOCK_ENTITY_SW);
                Remove(x - 1, z - 1, FLAG_BLOCK_ENTITY_NE);
            }
        } else if (type == 2) {
            if (rotation == 0) {
                Remove(x, z, FLAG_BLOCK_ENTITY_W | FLAG_BLOCK_ENTITY_N);
                Remove(x - 1, z, FLAG_BLOCK_ENTITY_E);
                Remove(x, z + 1, FLAG_BLOCK_ENTITY_S);
            } else if (rotation == 1) {
                Remove(x, z, FLAG_BLOCK_ENTITY_E | FLAG_BLOCK_ENTITY_N);
                Remove(x, z + 1, FLAG_BLOCK_ENTITY_S);
                Remove(x + 1, z, FLAG_BLOCK_ENTITY_W);
            } else if (rotation == 2) {
                Remove(x, z, FLAG_BLOCK_ENTITY_E | FLAG_BLOCK_ENTITY_S);
                Remove(x + 1, z, FLAG_BLOCK_ENTITY_W);
                Remove(x, z - 1, FLAG_BLOCK_ENTITY_N);
            } else if (rotation == 3) {
                Remove(x, z, FLAG_BLOCK_ENTITY_W | FLAG_BLOCK_ENTITY_S);
                Remove(x, z - 1, FLAG_BLOCK_ENTITY_N);
                Remove(x - 1, z, FLAG_BLOCK_ENTITY_E);
            }
        }
        if (projectiles) {
            if (type == 0) {
                if (rotation == 0) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_W);
                    Remove(x - 1, z, FLAG_BLOCK_PROJECTILE_E);
                } else if (rotation == 1) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_N);
                    Remove(x, z + 1, FLAG_BLOCK_PROJECTILE_S);
                } else if (rotation == 2) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_E);
                    Remove(x + 1, z, FLAG_BLOCK_PROJECTILE_W);
                } else if (rotation == 3) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_S);
                    Remove(x, z - 1, FLAG_BLOCK_PROJECTILE_N);
                }
            } else if ((type == 1) || (type == 3)) {
                if (rotation == 0) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_NW);
                    Remove(x - 1, z + 1, FLAG_BLOCK_PROJECTILE_SE);
                } else if (rotation == 1) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_NE);
                    Remove(x + 1, z + 1, FLAG_BLOCK_PROJECTILE_SW);
                } else if (rotation == 2) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_SE);
                    Remove(x + 1, z - 1, FLAG_BLOCK_PROJECTILE_NW);
                } else if (rotation == 3) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_SW);
                    Remove(x - 1, z - 1, FLAG_BLOCK_PROJECTILE_NE);
                }
            } else if (type == 2) {
                if (rotation == 0) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_W | FLAG_BLOCK_PROJECTILE_N);
                    Remove(x - 1, z, FLAG_BLOCK_PROJECTILE_E);
                    Remove(x, z + 1, FLAG_BLOCK_PROJECTILE_S);
                } else if (rotation == 1) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_E | FLAG_BLOCK_PROJECTILE_N);
                    Remove(x, z + 1, FLAG_BLOCK_PROJECTILE_S);
                    Remove(x + 1, z, FLAG_BLOCK_PROJECTILE_W);
                } else if (rotation == 2) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_E | FLAG_BLOCK_PROJECTILE_S);
                    Remove(x + 1, z, FLAG_BLOCK_PROJECTILE_W);
                    Remove(x, z - 1, FLAG_BLOCK_PROJECTILE_N);
                } else if (rotation == 3) {
                    Remove(x, z, FLAG_BLOCK_PROJECTILE_W | FLAG_BLOCK_PROJECTILE_S);
                    Remove(x, z - 1, FLAG_BLOCK_PROJECTILE_N);
                    Remove(x - 1, z, FLAG_BLOCK_PROJECTILE_E);
                }
            }
        }
    }

    void CollisionMap::Remove(int32_t rotation, int32_t objSizeX, int32_t x0, int32_t z0, int32_t objSizeZ, bool projectiles)
    {
        int32_t options = FLAG_BLOCK_ENTITY;

        if (projectiles) {
            options += FLAG_BLOCK_PROJECTILE;
        }

        if ((rotation == 1) || (rotation == 3)) {
            int32_t tmp = objSizeX;
            objSizeX = objSizeZ;
            objSizeZ = tmp;
        }
        for (int32_t x = x0; x < (x0 + objSizeX); x++) {
            if ((x >= 0) && (x < sizeX)) {
                for (int32_t z = z0; z < (z0 + objSizeZ); z++) {
                    if ((z >= 0) && (z < sizeZ)) {
                        Remove(x, z, options);
                    }
                }
            }
        }
    }

    void CollisionMap::Remove(int32_t x, int32_t z, int32_t options)
    {
        flags[x][z] &= 0xffffff - options;
    }

    void CollisionMap::RemoveSolid(int32_t x, int32_t z)
    {
        flags[x][z] &= 0xdfffff;
    }

    bool CollisionMap::ReachedDestination(int32_t sx, int32_t sz, int32_t dx, int32_t dz, int32_t rotation,
        int32_t type)
    {
        if ((sx == dx) && (sz == dz)) {
            return true;
        }

        if (type == 0) {
            if (rotation == 0) {
                if ((sx == (dx - 1)) && (sz == dz)) {
                    return true;
                } else if ((sx == dx) && (sz == (dz + 1)) && ((flags[sx][sz] & 0x1280120) == 0)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz - 1)) && ((flags[sx][sz] & 0x1280102) == 0);
                }
            } else if (rotation == 1) {
                if ((sx == dx) && (sz == (dz + 1))) {
                    return true;
                } else if ((sx == (dx - 1)) && (sz == dz) && ((flags[sx][sz] & 0x1280108) == 0)) {
                    return true;
                } else {
                    return (sx == (dx + 1)) && (sz == dz) && ((flags[sx][sz] & 0x1280180) == 0);
                }
            } else if (rotation == 2) {
                if ((sx == (dx + 1)) && (sz == dz)) {
                    return true;
                } else if ((sx == dx) && (sz == (dz + 1)) && ((flags[sx][sz] & 0x1280120) == 0)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz - 1)) && ((flags[sx][sz] & 0x1280102) == 0);
                }
            } else if (rotation == 3) {
                if ((sx == dx) && (sz == (dz - 1))) {
                    return true;
                } else if ((sx == (dx - 1)) && (sz == dz) && ((flags[sx][sz] & 0x1280108) == 0)) {
                    return true;
                } else {
                    return (sx == (dx + 1)) && (sz == dz) && ((flags[sx][sz] & 0x1280180) == 0);
                }
            }
        } else if (type == 2) {
            if (rotation == 0) {
                if ((sx == (dx - 1)) && (sz == dz)) {
                    return true;
                } else if ((sx == dx) && (sz == (dz + 1))) {
                    return true;
                } else if ((sx == (dx + 1)) && (sz == dz) && ((flags[sx][sz] & 0x1280180) == 0)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz - 1)) && ((flags[sx][sz] & 0x1280102) == 0);
                }
            } else if (rotation == 1) {
                if ((sx == (dx - 1)) && (sz == dz) && ((flags[sx][sz] & 0x1280108) == 0)) {
                    return true;
                } else if ((sx == dx) && (sz == (dz + 1))) {
                    return true;
                } else if ((sx == (dx + 1)) && (sz == dz)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz - 1)) && ((flags[sx][sz] & 0x1280102) == 0);
                }
            } else if (rotation == 2) {
                if ((sx == (dx - 1)) && (sz == dz) && ((flags[sx][sz] & 0x1280108) == 0)) {
                    return true;
                } else if ((sx == dx) && (sz == (dz + 1)) && ((flags[sx][sz] & 0x1280120) == 0)) {
                    return true;
                } else if ((sx == (dx + 1)) && (sz == dz)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz - 1));
                }
            } else if (rotation == 3) {
                if ((sx == (dx - 1)) && (sz == dz)) {
                    return true;
                } else if ((sx == dx) && (sz == (dz + 1)) && ((flags[sx][sz] & 0x1280120) == 0)) {
                    return true;
                } else if ((sx == (dx + 1)) && (sz == dz) && ((flags[sx][sz] & 0x1280180) == 0)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz - 1));
                }
            }
        } else if (type == 9) {
            if ((sx == dx) && (sz == (dz + 1)) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_S) == 0)) {
                return true;
            } else if ((sx == dx) && (sz == (dz - 1)) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_N) == 0)) {
                return true;
            } else if ((sx == (dx - 1)) && (sz == dz) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_E) == 0)) {
                return true;
            }
            return (sx == (dx + 1)) && (sz == dz) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_W) == 0);
        }
        return false;
    }

    bool CollisionMap::ReachedWall(int32_t sx, int32_t sz, int32_t dx, int32_t dz, int32_t type, int32_t rotation)
    {
        if ((sx == dx) && (sz == dz)) {
            return true;
        }
        if ((type == 6) || (type == 7)) {
            if (type == 7) {
                rotation = (rotation + 2) & 3;
            }
            if (rotation == 0) {
                if ((sx == (dx + 1)) && (sz == dz) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_W) == 0)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz - 1)) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_N) == 0);
                }
            } else if (rotation == 1) {
                if ((sx == (dx - 1)) && (sz == dz) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_E) == 0)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz - 1)) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_N) == 0);
                }
            } else if (rotation == 2) {
                if ((sx == (dx - 1)) && (sz == dz) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_E) == 0)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz + 1)) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_S) == 0);
                }
            } else if (rotation == 3) {
                if ((sx == (dx + 1)) && (sz == dz) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_W) == 0)) {
                    return true;
                } else {
                    return (sx == dx) && (sz == (dz + 1)) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_S) == 0);
                }
            }
        } else if (type == 8) {
            if ((sx == dx) && (sz == (dz + 1)) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_S) == 0)) {
                return true;
            } else if ((sx == dx) && (sz == (dz - 1)) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_N) == 0)) {
                return true;
            } else if ((sx == (dx - 1)) && (sz == dz) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_E) == 0)) {
                return true;
            }
            return (sx == (dx + 1)) && (sz == dz) && ((flags[sx][sz] & FLAG_BLOCK_ENTITY_W) == 0);
        }
        return false;
    }

    bool CollisionMap::ReachedLoc(int32_t srcX, int32_t srcZ, int32_t dstX, int32_t dstZ, int32_t dstSizeX,
        int32_t dstSizeZ, int32_t interactionSides)
    {
        int32_t maxX = (dstX + dstSizeX) - 1;
        int32_t maxZ = (dstZ + dstSizeZ) - 1;

        if ((srcX >= dstX) && (srcX <= maxX) && (srcZ >= dstZ) && (srcZ <= maxZ)) {
            return true;
        } else if ((srcX == (dstX - 1)) && (srcZ >= dstZ) && (srcZ <= maxZ) && ((flags[srcX][srcZ] & FLAG_BLOCK_ENTITY_E) == 0) && ((interactionSides & 8) == 0)) {
            return true;
        } else if ((srcX == (maxX + 1)) && (srcZ >= dstZ) && (srcZ <= maxZ) && ((flags[srcX][srcZ] & FLAG_BLOCK_ENTITY_W) == 0) && ((interactionSides & 2) == 0)) {
            return true;
        } else if ((srcZ == (dstZ - 1)) && (srcX >= dstX) && (srcX <= maxX) && ((flags[srcX][srcZ] & FLAG_BLOCK_ENTITY_N) == 0) && ((interactionSides & 4) == 0)) {
            return true;
        }
        return (srcZ == (maxZ + 1)) && (srcX >= dstX) && (srcX <= maxX) && ((flags[srcX][srcZ] & FLAG_BLOCK_ENTITY_S) == 0) && ((interactionSides & 1) == 0);
    }
}
