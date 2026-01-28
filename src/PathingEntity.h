#pragma once
#include "PCH.h"
#include "Entity.h"

namespace SDL_Client
{
    class PathingEntity : public Entity
    {
    public:
        void Move(int32_t x, int32_t z, bool teleport);
        void Step(bool running, int32_t direction);
        void Hit(int32_t type, int32_t damage);
        void ResetPath();
        virtual bool IsVisible();

    public:
        int32_t cycle = 0;
        int32_t x = 0;
        int32_t z = 0;
        int32_t y = 0;
        int32_t height = 200;
        int32_t yaw = 0;
        int32_t dstYaw = 0;

        int32_t forceMoveStartSceneTileX = 0;
        int32_t forceMoveStartSceneTileZ = 0;
        int32_t forceMoveEndSceneTileX = 0;
        int32_t forceMoveEndSceneTileZ = 0;
        int32_t forceMoveEndCycle = 0;
        int32_t forceMoveStartCycle = 0;
        int32_t forceMoveFaceDirection = 0;
        int32_t targetTileX = 0;
        int32_t targetTileZ = 0;

        int32_t chatColor = 0;
        int32_t chatStyle = 0;
        int32_t chatTimer = 100;

        int32_t size = 1;
        int32_t primarySeqID = -1;
        int32_t secondarySeqFrame = 0;
        int32_t secondarySeqID = -1;
        int32_t seqPathLength = 0;
        int32_t primarySeqDelay = 0;
        int32_t primarySeqFrame = 0;
        int32_t primarySeqCycle = 0;
        int32_t primarySeqLoop = 0;
        int32_t secondarySeqCycle = 0;
        int32_t pathLength = 0;
        int32_t seqStandID = -1;
        int32_t seqTrigger = 0;

        int32_t spotanimID = -1;
        int32_t spotanimFrame = 0;
        int32_t spotanimCycle = 0;
        int32_t spotanimLastCycle = 0;
        int32_t spotanimOffset = 0;

        int32_t seqTurnAroundID = -1;
        int32_t seqWalkID = -1;
        int32_t seqTurnLeftID = -1;
        int32_t seqTurnRightID = -1;
        int32_t seqTurnID = -1;
        int32_t seqRunID = -1;
        int32_t targetID = -1;
        int32_t turnSpeed = 32;

        int32_t combatCycle = -1000;
        int32_t health = 0;
        int32_t totalHealth = 0;

        std::array<int32_t, 10> pathTileX{};
        std::array<int32_t, 10> pathTileZ{};
        std::array<int32_t, 4> damage{};
        std::array<int32_t, 4> damageType{};
        std::array<int32_t, 4> damageCycle{};
        std::array<int8_t, 10> pathRunning{};
        std::string chat;

        /**
 * Passed to {@link Scene#addTemporary(Entity, int, int, int, int, int, int, boolean, int)} to provide an additional
 * tile worth of draw padding ahead of this entity for things like animations that extend past the normal boundary.
 */
        bool needsForwardDrawPadding = false;
    };
}
