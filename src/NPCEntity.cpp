#include "NPCEntity.h"

#include "SeqTransform.h"
#include "SeqType.h"
#include "SpotAnimType.h"

namespace SDL_Client
{
    std::shared_ptr<Model> NPCEntity::GetModel()
    {
        if (type == nullptr) {
            return nullptr;
        }

        auto model = GetSequencedModel();
        if (model == nullptr) {
            return nullptr;
        }

        height = model->minY;

        if ((spotanimID != -1) && (spotanimFrame != -1)) {
            auto& spot = SpotAnimType::instances[spotanimID];
            const auto& spotModel0 = spot.GetModel();

            if (spotModel0 != nullptr) {
                int32_t transformID = spot.seq.transformIDs[spotanimFrame];

                auto spotModel1 = std::make_shared<Model>(true, SeqTransform::IsNull(transformID), false, *spotModel0);
                spotModel1->Translate(0, -spotanimOffset, 0);
                spotModel1->CreateLabelReferences();
                spotModel1->ApplyTransform(transformID);
                spotModel1->labelFaces.clear();
                spotModel1->labelVertices.clear();

                if ((spot.scaleXY != 128) || (spot.scaleZ != 128)) {
                    spotModel1->Scale(spot.scaleXY, spot.scaleXY, spot.scaleZ);
                }

                spotModel1->CalculateNormals(64 + spot.lightAmbient, 850 + spot.lightAttenuation, -30, -50, -30, true);
                model = std::make_shared<Model>(2, -819, std::vector{model, spotModel1});
            }
        }

        if (type->size == 1) {
            model->pickable = true;
        }

        return model;
    }

    bool NPCEntity::IsVisible()
    {
        return type != nullptr;
    }

    std::shared_ptr<Model> NPCEntity::GetSequencedModel() const
    {
        if ((primarySeqID >= 0) && (primarySeqDelay == 0)) {
            int32_t primaryTransformID = SeqType::instances[primarySeqID].transformIDs[primarySeqFrame];
            int32_t secondaryTransformID = -1;

            if ((secondarySeqID >= 0) && (secondarySeqID != seqStandID)) {
                secondaryTransformID = SeqType::instances[secondarySeqID].transformIDs[secondarySeqFrame];
            }

            return type->GetSequencedModel(secondaryTransformID, primaryTransformID, SeqType::instances[primarySeqID].mask);
        }

        int32_t transformID = -1;

        if (secondarySeqID >= 0) {
            transformID = SeqType::instances[secondarySeqID].transformIDs[secondarySeqFrame];
        }

        std::vector<int32_t> empty;
        return type->GetSequencedModel(-1, transformID, empty);
    }
}