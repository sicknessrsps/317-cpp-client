#include "PathingEntity.h"
#include "SeqType.h"
#include "Game.h"

namespace SDL_Client
{
    void PathingEntity::Move(int32_t x, int32_t z, bool teleport)
    {
        if ((primarySeqID != -1) && (SeqType::instances[primarySeqID].idleStyle == 1)) {
            primarySeqID = -1;
        }
        if (!teleport) {
            int32_t dx = x - pathTileX[0];
            int32_t dz = z - pathTileZ[0];

            if ((dx >= -8) && (dx <= 8) && (dz >= -8) && (dz <= 8)) {
                if (pathLength < 9) {
                    pathLength++;
                }

                for (int32_t i = pathLength; i > 0; i--) {
                    pathTileX[i] = pathTileX[i - 1];
                    pathTileZ[i] = pathTileZ[i - 1];
                    pathRunning[i] = pathRunning[i - 1];
                }

                pathTileX[0] = x;
                pathTileZ[0] = z;
                pathRunning[0] = false;
                return;
            }
        }
        pathLength = 0;
        seqPathLength = 0;
        seqTrigger = 0;
        pathTileX[0] = x;
        pathTileZ[0] = z;
        this->x = (pathTileX[0] * 128) + (size * 64);
        this->z = (pathTileZ[0] * 128) + (size * 64);
    }

    void PathingEntity::Step(bool running, int32_t direction)
    {
        int32_t nextX = pathTileX[0];
        int32_t nextZ = pathTileZ[0];

        if (direction == 0) {
            nextX--;
            nextZ++;
        } else if (direction == 1) {
            nextZ++;
        } else if (direction == 2) {
            nextX++;
            nextZ++;
        } else if (direction == 3) {
            nextX--;
        } else if (direction == 4) {
            nextX++;
        } else if (direction == 5) {
            nextX--;
            nextZ--;
        } else if (direction == 6) {
            nextZ--;
        } else if (direction == 7) {
            nextX++;
            nextZ--;
        }

       if ((primarySeqID != -1) && (SeqType::instances[primarySeqID].idleStyle == 1)) {
            primarySeqID = -1;
        }

        if (pathLength < 9) {
            pathLength++;
        }

        for (int i = pathLength; i > 0; i--) {
            pathTileX[i] = pathTileX[i - 1];
            pathTileZ[i] = pathTileZ[i - 1];
            pathRunning[i] = pathRunning[i - 1];
        }

        pathTileX[0] = nextX;
        pathTileZ[0] = nextZ;
        pathRunning[0] = running;
    }

    void PathingEntity::Hit(int32_t type, int32_t damage)
    {
        for (int32_t i = 0; i < 4; i++) {
            if (damageCycle[i] <= Game::loopCycle) {
                this->damage[i] = damage;
                damageType[i] = type;
                damageCycle[i] = Game::loopCycle + 70;
                return;
            }
        }
    }

    void PathingEntity::ResetPath()
    {
        pathLength = 0;
        seqPathLength = 0;
    }

    bool PathingEntity::IsVisible()
    {
        return false;
    }
}
