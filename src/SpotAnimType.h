#pragma once
#include "PCH.h"
#include "FileArchive.h"
#include "Buffer.h"
#include "Model.h"
#include "SeqType.h"

namespace SDL_Client
{
    class SpotAnimType
    {
    public:
        static void Unpack(FileArchive& archive);
        std::shared_ptr<Model> GetModel();
    public:
        static int32_t count;
        static std::vector<SpotAnimType> instances;
        static LRUMap<int32_t, Model> modelCache;

        std::array<int32_t, 6> colorSrc{};
        std::array<int32_t, 6> colorDst{};
        int32_t index;
        int32_t modelID;
        int32_t seqID = -1;
        SeqType seq;
        int32_t scaleXY = 128;
        int32_t scaleZ = 128;
        int32_t rotation;
        int32_t lightAmbient;
        int32_t lightAttenuation;
    private:
        void Read(Buffer& buffer);
    };
}
