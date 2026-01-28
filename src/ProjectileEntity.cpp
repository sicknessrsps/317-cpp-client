#include "ProjectileEntity.h"
#include "SeqTransform.h"
#include <cmath>

namespace SDL_Client
{
    ProjectileEntity::ProjectileEntity(int32_t peakPitch, int32_t offsetY, int32_t startCycle,
                                       int32_t lastCycle, int32_t arc, int32_t level, int32_t srcY,
                                       int32_t srcZ, int32_t srcX, int32_t target, int32_t spotanimID)
        : startCycle(startCycle), lastCycle(lastCycle), srcX(srcX), srcZ(srcZ), srcY(srcY),
          offsetY(offsetY), peakPitch(peakPitch), arc(arc), target(target), level(level),
          spotanim(SpotAnimType::instances[spotanimID])
    {
    }

    void ProjectileEntity::UpdateVelocity(int32_t cycle, int32_t dstZ, int32_t dstY, int32_t dstX)
    {
        if (!mobile) {
            float dx = static_cast<float>(dstX - srcX);
            float dz = static_cast<float>(dstZ - srcZ);
            float d = std::sqrt((dx * dx) + (dz * dz));
            x = static_cast<float>(srcX) + ((dx * static_cast<float>(arc)) / d);
            z = static_cast<float>(srcZ) + ((dz * static_cast<float>(arc)) / d);
            y = static_cast<float>(srcY);
        }

        float dt = static_cast<float>((lastCycle + 1) - cycle);
        velocityX = (static_cast<float>(dstX) - x) / dt;
        velocityZ = (static_cast<float>(dstZ) - z) / dt;
        velocity = std::sqrt((velocityX * velocityX) + (velocityZ * velocityZ));

        if (!mobile) {
            velocityY = -velocity * std::tan(static_cast<float>(peakPitch) * 0.02454369f);
        }

        accelerationY = (2.0f * (static_cast<float>(dstY) - y - (velocityY * dt))) / (dt * dt);
    }

    std::shared_ptr<Model> ProjectileEntity::GetModel()
    {
        auto tmp = spotanim.GetModel();
        if (tmp == nullptr) {
            return nullptr;
        }

        int32_t transformID = -1;

        if (spotanim.seqID != -1) {
            transformID = spotanim.seq.transformIDs[seqFrame];
        }

        auto model = std::make_shared<Model>(true, SeqTransform::IsNull(transformID), false, *tmp);

        if (transformID != -1) {
            model->CreateLabelReferences();
            model->ApplyTransform(transformID);
            model->labelFaces.clear();
            model->labelVertices.clear();
        }

        if ((spotanim.scaleXY != 128) || (spotanim.scaleZ != 128)) {
            model->Scale(spotanim.scaleXY, spotanim.scaleXY, spotanim.scaleZ);
        }

        model->RotateX(pitch);
        model->CalculateNormals(64 + spotanim.lightAmbient, 850 + spotanim.lightAttenuation, -30, -50, -30, true);

        return model;
    }

    void ProjectileEntity::Update(int32_t delta)
    {
        mobile = true;
        x += velocityX * static_cast<float>(delta);
        z += velocityZ * static_cast<float>(delta);
        y += (velocityY * static_cast<float>(delta)) + (0.5f * accelerationY * static_cast<float>(delta) * static_cast<float>(delta));
        velocityY += accelerationY * static_cast<float>(delta);

        yaw = (static_cast<int32_t>(std::atan2(velocityX, velocityZ) * 325.949f) + 1024) & 0x7ff;
        pitch = static_cast<int32_t>(std::atan2(velocityY, velocity) * 325.949f) & 0x7ff;

        if (spotanim.seqID != -1) {
            for (seqCycle += delta; seqCycle > spotanim.seq.GetFrameDuration(seqFrame); ) {
                seqCycle -= spotanim.seq.GetFrameDuration(seqFrame) + 1;
                seqFrame++;
                if (seqFrame >= spotanim.seq.frameCount) {
                    seqFrame = 0;
                }
            }
        }
    }
}