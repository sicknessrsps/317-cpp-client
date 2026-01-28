#include "IdkType.h"
#include "Buffer.h"

namespace SDL_Client
{

    std::vector<IdkType> IdkType::instances;

    void IdkType::Unpack(FileArchive& archive)
    {
        Buffer buffer(archive.Read("idk.dat"));
        count = buffer.ReadU16();
        if (instances.empty()) {
            instances.resize(count);
        }
        for (int32_t j = 0; j < count; j++) {
            instances[j] = IdkType();
            instances[j].Read(buffer);
        }
    }

    std::shared_ptr<Model> IdkType::GetModel() const
    {
        if (modelIDs.empty()) {
            return nullptr;
        }
        std::vector<std::shared_ptr<Model>> models(modelIDs.size());
        for (int32_t i = 0; i < modelIDs.size(); i++) {
            models[i] = Model::TryGet(modelIDs[i]);
        }

        std::shared_ptr<Model> model;

        if (models.size() == 1) {
            model = models[0];
        } else {
            model = std::make_shared<Model>(models.size(), models);
        }

        for (int32_t i = 0; i < 6; i++) {
            if (colorSrc[i] == 0) {
                break;
            }
            model->Recolor(colorSrc[i], colorDst[i]);
        }

        return model;
    }

    std::shared_ptr<Model> IdkType::GetHeadModel() const
    {
        std::vector<std::shared_ptr<Model>> models(5);
        int32_t i = 0;
        for (int32_t j = 0; j < 5; j++) {
            if (headModelIDs[j] != -1) {
                models[i++] = Model::TryGet(headModelIDs[j]);
            }
        }
        auto model = std::make_shared<Model>(i, models);
        for (int32_t k = 0; k < 6; k++) {
            if (colorSrc[k] == 0) {
                break;
            }
            model->Recolor(colorSrc[k], colorDst[k]);
        }
        return model;
    }

    bool IdkType::ValidateHeadModel() const
    {
        bool loaded = true;
        for (int32_t i = 0; i < 5; i++) {
            if ((headModelIDs[i] != -1) && !Model::Validate(headModelIDs[i])) {
                loaded = false;
            }
        }
        return loaded;
    }

    bool IdkType::ValidateModel() const
    {
        if (modelIDs.empty()) {
            return true;
        }
        bool loaded = true;
        for (int32_t modelID : modelIDs) {
            if (!Model::Validate(modelID)) {
                loaded = false;
            }
        }
        return loaded;
    }

    void IdkType::Read(Buffer& in)
    {
        while (true) {
            int32_t code = in.ReadU8();
            if (code == 0) {
                return;
            } else if (code == 1) {
                type = in.ReadU8();
            } else if (code == 2) {
                int32_t j = in.ReadU8();
                modelIDs.resize(j);
                for (int32_t k = 0; k < j; k++) {
                    modelIDs[k] = in.ReadU16();
                }
            } else if (code == 3) {
                selectable = true;
            } else if ((code >= 40) && (code < 50)) {
                colorSrc[code - 40] = in.ReadU16();
            } else if ((code >= 50) && (code < 60)) {
                colorDst[code - 50] = in.ReadU16();
            } else if ((code >= 60) && (code < 70)) {
                headModelIDs[code - 60] = in.ReadU16();
            } else {
                LOG_ERROR("Error unrecognised identikit config code: %i", code);
            }
        }
    }
}
