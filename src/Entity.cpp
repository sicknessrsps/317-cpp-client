#include "Entity.h"
#include "Model.h"

namespace SDL_Client {

    void Entity::Draw(
        int32_t yaw,
        int32_t sinEyePitch, int32_t cosEyePitch,
        int32_t sinEyeYaw,  int32_t cosEyeYaw,
        int32_t relativeX,  int32_t relativeY,  int32_t relativeZ,
        int32_t bitset
    ) {
        const auto& model = GetModel();
        if (model != nullptr) {
            minY = model->minY;
            model->Draw(yaw, sinEyePitch, cosEyePitch, sinEyeYaw, cosEyeYaw, relativeX, relativeY, relativeZ, bitset);
        }
    }

    std::shared_ptr<Model> Entity::GetModel() {
        return nullptr;
    }

}