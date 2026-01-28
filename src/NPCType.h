#pragma once

#include "PCH.h"
#include "FileArchive.h"
#include "Buffer.h"
#include "Model.h"

namespace SDL_Client
{
    class Game;

    class NPCType
    {
    public:
        static void Unpack(FileArchive& archive);
        static void Unload();
        static std::shared_ptr<NPCType> Get(int32_t id);
    public:
        std::shared_ptr<Model> GetHeadModel();
        std::shared_ptr<NPCType> GetOverrideType() const;
        std::shared_ptr<Model> GetSequencedModel(int32_t secondaryTransformID, int32_t primaryTransformID, std::vector<int32_t>& seqMask);

        inline static int32_t count = 0;
        static LRUMap<int64_t, Model> modelCache;
        static Game* game;

    public:

        std::vector<std::string> options;
        std::vector<int32_t> colorDst;
        std::vector<int32_t> colorSrc;
        std::vector<int32_t> headModelIDs;
        std::vector<int32_t> overrides;
        std::vector<int32_t> modelIDs;

        std::string name;
        std::string examine;

        int64_t uid = -1L;

        int32_t seqTurnRightID = -1;
        int32_t varbit = -1;
        int32_t seqTurnAroundID = -1;
        int32_t varp = -1;
        int32_t level = -1;
        int32_t seqWalkID = -1;
        int32_t headicon = -1;
        int32_t seqStandID = -1;
        int32_t turnSpeed = 32;
        int32_t seqTurnLeftID = -1;
        int32_t lightAmbient = 0;
        int32_t scaleZ = 128;
        int32_t scaleXY = 128;
        int32_t lightAttenuation = 0;

        int8_t size = 1;
        bool interactable = true;
        bool showOnMinimap = true;
        bool important = false;

    private:
        void Read(Buffer& in);

        inline static Buffer dat;
        inline static std::vector<int32_t> offsets;
        inline static int32_t cachePos = 0;
    };
}
