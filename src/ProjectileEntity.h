#pragma once
#include "Entity.h"
#include "SpotAnimType.h"

namespace SDL_Client
{
    class ProjectileEntity : public Entity
    {
    public:
        ProjectileEntity(int32_t peakPitch, int32_t offsetY, int32_t startCycle, int32_t lastCycle,
                        int32_t arc, int32_t level, int32_t srcY, int32_t srcZ, int32_t srcX,
                        int32_t target, int32_t spotanimID);

        std::shared_ptr<Model> GetModel() override;
        void UpdateVelocity(int32_t cycle, int32_t dstZ, int32_t dstY, int32_t dstX);
        void Update(int32_t delta);

    public:
        const int32_t startCycle;
        const int32_t lastCycle;
        const int32_t srcX;
        const int32_t srcZ;
        const int32_t srcY;
        const int32_t offsetY;
        const int32_t peakPitch;
        const int32_t arc;
        const int32_t target;
        const int32_t level;

        SpotAnimType spotanim;

        float velocityX = 0.0f;
        float velocityZ = 0.0f;
        float velocity = 0.0f;
        float velocityY = 0.0f;
        float accelerationY = 0.0f;
        bool mobile = false;
        float x = 0.0f;
        float z = 0.0f;
        float y = 0.0f;
        int32_t seqFrame = 0;
        int32_t seqCycle = 0;
        int32_t yaw = 0;
        int32_t pitch = 0;
    };
}