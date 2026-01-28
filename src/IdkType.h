#pragma once

#include "PCH.h"
#include "FileArchive.h"
#include "Buffer.h"
#include "Model.h"

namespace SDL_Client
{
    class IdkType
    {
    public:
        static void Unpack(FileArchive& archive);
        std::shared_ptr<Model> GetModel() const;
        std::shared_ptr<Model> GetHeadModel() const;
        bool ValidateModel() const;
        bool ValidateHeadModel() const;
    public:
        inline static int32_t count = 0;
        int32_t type = -1;
        std::vector<int32_t> modelIDs;
        bool selectable = false;
        std::array<int32_t, 6> colorSrc{};
        std::array<int32_t, 6> colorDst{};
        std::vector<int32_t> headModelIDs = {-1, -1, -1, -1, -1};
        static std::vector<IdkType> instances;
    private:
        void Read(Buffer& in);
    };
}