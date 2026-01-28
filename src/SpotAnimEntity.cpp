#include "SpotAnimEntity.h"
#include "SeqTransform.h"

namespace SDL_Client
{
    SpotAnimEntity::SpotAnimEntity(int32_t level, int32_t cycle, int32_t delay, int32_t id, int32_t y, int32_t z,
        int32_t x) : level(level), x(x), z(z), y(y), startCycle(cycle + delay), type(SpotAnimType::instances[id])
    {}

    std::shared_ptr<Model> SpotAnimEntity::GetModel()
    {
        auto base = type.GetModel();

        if (base == nullptr) {
            return nullptr;
        }

        int32_t transformID = type.seq.transformIDs[seqFrame];

        auto model = std::make_shared<Model>(true, SeqTransform::IsNull(transformID), false, *base);

        if (!seqComplete) {
            model->CreateLabelReferences();
            model->ApplyTransform(transformID);
            model->labelFaces.clear();
            model->labelVertices.clear();
        }

        if ((type.scaleXY != 128) || (type.scaleZ != 128)) {
            model->Scale(type.scaleXY, type.scaleXY, type.scaleZ);
        }

        if (type.rotation != 0) {
            if (type.rotation == 90) {
                model->RotateY90();
            }
            if (type.rotation == 180) {
                model->RotateY90();
                model->RotateY90();
            }
            if (type.rotation == 270) {
                model->RotateY90();
                model->RotateY90();
                model->RotateY90();
            }
        }
        model->CalculateNormals(64 + type.lightAmbient, 850 + type.lightAttenuation, -30, -50, -30, true);
        return model;
    }

    void SpotAnimEntity::Update(int32_t delta)
    {
        for (seqCycle += delta; seqCycle > type.seq.GetFrameDuration(seqFrame); ) {
            seqCycle -= type.seq.GetFrameDuration(seqFrame) + 1;
            seqFrame++;

            if (seqFrame >= type.seq.frameCount) {
                seqFrame = 0;
                seqComplete = true;
            }
        }
    }
}
