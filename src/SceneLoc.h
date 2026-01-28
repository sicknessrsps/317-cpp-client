#pragma once
#include "PCH.h"

namespace SDL_Client
{
    class Entity;

    class SceneLoc {
    public:
        int32_t level = 0;
        int32_t y = 0;
        int32_t x = 0;
        int32_t z = 0;
        std::shared_ptr<Entity> entity;
        int32_t yaw = 0;
        int32_t minSceneTileX = 0;
        int32_t maxSceneTileX = 0;
        int32_t minSceneTileZ = 0;
        int32_t maxSceneTileZ = 0;
        int32_t distance = 0;
        int32_t cycle = 0;
        int32_t bitset = 0;
        int8_t info = 0;

        [[nodiscard]] bool Drawn() const;

        bool operator==(SceneLoc const& other) const {
            return level == other.level &&
                   x == other.x &&
                   y == other.y &&
                   z == other.z &&
                   yaw == other.yaw &&
                   // bitset == other.bitset &&
                   entity == other.entity;
        }

        bool operator!=(SceneLoc const& other) const {
            return !(*this == other);
        }

        //SceneLoc() = default;
    };

}