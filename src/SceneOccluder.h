#include "PCH.h"

namespace SDL_Client
{

    /**
    * A wall occluder which runs along the X axis.
    */
    inline constexpr int32_t TYPE_WALL_X = 1;
    /**
     * A wall occluder which runs along the Z axis.
     */
    inline constexpr int32_t TYPE_WALL_Z = 2;
    /**
     * A ground occluder which covers the XZ plane.
     */
    inline constexpr int32_t TYPE_GROUND = 4;

    struct SceneOccluder
    {
        int32_t minTileX = 0;
        int32_t maxTileX = 0;
        int32_t minTileZ = 0;
        int32_t maxTileZ = 0;
        int32_t type = 0;
        int32_t minX = 0;
        int32_t maxX = 0;
        int32_t minZ = 0;
        int32_t maxZ = 0;
        int32_t minY = 0;
        int32_t maxY = 0;
        int32_t mode = 0;
        int32_t minDeltaX = 0;
        int32_t maxDeltaX = 0;
        int32_t minDeltaZ = 0;
        int32_t maxDeltaZ = 0;
        int32_t minDeltaY = 0;
        int32_t maxDeltaY = 0;

        bool operator==(const SceneOccluder&) const = default;
    };
}