#pragma once
#include "PCH.h"
#include "SceneLoc.h"

namespace SDL_Client {

    struct SceneTileUnderlay {
        int32_t southwestColor = 0;
        int32_t southeastColor = 0;
        int32_t northeastColor = 0;
        int32_t northwestColor = 0;
        int32_t textureID = 0;
        int32_t rgb = 0;
        bool flat = false;
        bool initialized = false;
    };

    struct SceneGroundDecoration
    {
        std::shared_ptr<Entity> entity;
        int32_t y = 0;
        int32_t x = 0;
        int32_t z = 0;
        int32_t bitset = 0;
        int8_t info = 0;
    };

    struct SceneWall
    {
        std::shared_ptr<Entity> entityA;
        std::shared_ptr<Entity> entityB;
        int32_t y = 0;
        int32_t x = 0;
        int32_t z = 0;
        int32_t typeA = 0;
        int32_t typeB = 0;
        int32_t bitset = 0;
        int8_t info = 0;
    };

    struct SceneWallDecoration
    {
        std::shared_ptr<Entity> entity;
        int32_t y = 0;
        int32_t x = 0;
        int32_t z = 0;
        int32_t type = 0;
        int32_t rotation = 0;
        int32_t bitset = 0;
        int8_t info = 0;
    };

    struct SceneObjStack
    {
        std::shared_ptr<Entity> topObj;
        std::shared_ptr<Entity> bottomObj;
        std::shared_ptr<Entity> middleObj;
        int32_t y = 0;
        int32_t x = 0;
        int32_t z = 0;
        int32_t bitset = 0;
        int32_t offset = 0;
    };

    class SceneTileOverlay {
    public:
        static const JaggedArray<int8_t> SHAPE_POINTS;
        static const JaggedArray<int8_t> SHAPE_PATHS;

        static std::vector<int32_t> tmpScreenX;
        static std::vector<int32_t> tmpScreenY;
        static std::vector<int32_t> tmpViewspaceX;
        static std::vector<int32_t> tmpViewspaceY;
        static std::vector<int32_t> tmpViewspaceZ;

        std::vector<int32_t> vertexX;
        std::vector<int32_t> vertexY;
        std::vector<int32_t> vertexZ;
        std::vector<int32_t> triangleColorA;
        std::vector<int32_t> triangleColorB;
        std::vector<int32_t> triangleColorC;
        std::vector<int32_t> triangleVertexA;
        std::vector<int32_t> triangleVertexB;
        std::vector<int32_t> triangleVertexC;
        std::vector<int32_t> triangleTextureIDs; // May be empty

        int32_t shape = 0;
        int32_t rotation = 0;
        int32_t backgroundRGB = 0;
        int32_t foregroundRGB = 0;

        bool initialized = false;
        bool flat = false;

        SceneTileOverlay() = default;
        SceneTileOverlay(int32_t tileZ, int32_t southwestColor2, int32_t northwestColor1, int32_t northeastY,
            int32_t textureID, int32_t northeastColor2, int32_t rotation, int32_t southwestColor1,
            int32_t backgroundRGB, int32_t northeastColor1, int32_t northwestY, int32_t southeastY, int32_t southwestY,
            int32_t shape, int32_t northwestColor2, int32_t southeastColor2, int32_t southeastColor1,
            int32_t tileX, int32_t foregroundRGB);
    };

    class SceneLocTemporary : public DoublyLinkedList::Node
    {
        public:
            int32_t id = 0;
            int32_t rotation = 0;
            int32_t kind = 0;
            int32_t duration = -1;
            int32_t level = 0;
            int32_t classID = 0;
            int32_t localX = 0;
            int32_t localZ = 0;
            int32_t previousLocID = 0;
            int32_t previousRotation = 0;
            int32_t previousKind = 0;
            int32_t delay = 0;
    };

    class SceneTile : public DoublyLinkedList::Node {
    public:
        SceneTile(int32_t level, int32_t x, int32_t z);
        SceneTile() = default;

    public:
        SceneTileOverlay overlay;
        SceneTileUnderlay underlay;

        std::shared_ptr<SceneWall> wall;
        std::shared_ptr<SceneWallDecoration> wallDecoration;
        std::shared_ptr<SceneGroundDecoration> groundDecoration;
        std::shared_ptr<SceneTile> bridge;
        std::shared_ptr<SceneObjStack> objStack;

        std::array<std::shared_ptr<SceneLoc>, 5> locs{};
        std::array<int32_t, 5> locSpan{};

        /**
         * When larger than 1x1 locs reside on a tile, we have to know which part of it might be on this tile to properly
         * cull it.
         *
         * 0b0001 = x > loc.minSceneTileX
         * 0b0010 = z < loc.maxSceneTileZ
         * 0b0100 = x < loc.maxSceneTileX
         * 0b1000 = z > loc.minSceneTileZ
         */
        int32_t locSpans = 0;
        int32_t blockLocSpans = 0;
        int32_t inverseBlockLocSpans = 0;
        int32_t backWallTypes = 0;
        int32_t occludeLevel = 0;
        int32_t x = 0;
        int32_t z = 0;
        int32_t level = 0;
        int32_t drawLevel = 0;
        int32_t locCount = 0;
        int32_t checkLocSpans = 0;

        bool containsLocs = false;
        bool visible = false;
        bool update = false;
        bool initialized = false;
    };

}
