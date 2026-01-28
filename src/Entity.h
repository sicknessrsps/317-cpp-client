#pragma once
#include "PCH.h"

namespace SDL_Client {

    struct VertexNormal {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;
        int32_t w = 0;
    };

    class Model;

    class Entity : public DoublyLinkedList::Node {
    public:
        virtual ~Entity() = default;

        virtual void Draw(int32_t yaw, int32_t sinEyePitch, int32_t cosEyePitch, int32_t sinEyeYaw,
                          int32_t cosEyeYaw, int32_t relativeX, int32_t relativeY, int32_t relativeZ, int32_t bitset);

        virtual std::shared_ptr<Model> GetModel();

        int32_t minY = 1000;
        std::vector<VertexNormal> vertexNormal;
    };
}