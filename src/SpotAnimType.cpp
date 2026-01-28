#include "SpotAnimType.h"
#include <iostream>

namespace SDL_Client
{
    int32_t SpotAnimType::count;
    std::vector<SpotAnimType> SpotAnimType::instances;
    LRUMap<int32_t, Model> SpotAnimType::modelCache(30);

    std::shared_ptr<Model> SpotAnimType::GetModel()
    {
        auto model = modelCache.get(index);

        if (model != nullptr) {
            return model;
        }

        model = Model::TryGet(modelID);

        if (model == nullptr) {
            return nullptr;
        }

        for (int32_t i = 0; i < 6; i++) {
            if (colorSrc[i] != 0) {
                model->Recolor(colorSrc[i], colorDst[i]);
            }
        }

        modelCache.put(index, model);
        return model;
    }

    void SpotAnimType::Unpack(FileArchive& archive)
    {
        Buffer buffer(archive.Read("spotanim.dat"));
        count = buffer.ReadU16();

        if (instances.empty()) {
            instances.resize(count);
        }

        for (int32_t i = 0; i < count; i++) {
            instances[i].index = i;
            instances[i].Read(buffer);
        }
    }

    void SpotAnimType::Read(Buffer& buffer)
    {
        while (true) {
            int32_t code = buffer.ReadU8();
            if (code == 0) {
                return;
            } else if (code == 1) {
                modelID = buffer.ReadU16();
            } else if (code == 2) {
                seqID = buffer.ReadU16();
                if (!SeqType::instances.empty() && seqID < SeqType::instances.size()) {
                    seq = SeqType::instances[seqID];
                }
            } else if (code == 4) {
                scaleXY = buffer.ReadU16();
            } else if (code == 5) {
                scaleZ = buffer.ReadU16();
            } else if (code == 6) {
                rotation = buffer.ReadU16();
            } else if (code == 7) {
                lightAmbient = buffer.ReadU8();
            } else if (code == 8) {
                lightAttenuation = buffer.ReadU8();
            } else if ((code >= 40) && (code < 50)) {
                colorSrc[code - 40] = buffer.ReadU16();
            } else if ((code >= 50) && (code < 60)) {
                colorDst[code - 50] = buffer.ReadU16();
            } else {
                LOG_ERROR("Error unrecognised spotanim config code: %i", code);
            }
        }
    }

}