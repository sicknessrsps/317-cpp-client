#pragma once
#include "PCH.h"
#include "OnDemand.h"
#include "Draw3D.h"
#include "Entity.h"

namespace SDL_Client {

    class IfType;

    struct Header {
        std::vector<int8_t> data;
        int32_t vertexCount = 0;
        int32_t faceCount = 0;
        int32_t texturedFaceCount = 0;
        int32_t vertexFlagsOffset = 0;
        int32_t vertexXOffset = 0;
        int32_t vertexYOffset = 0;
        int32_t vertexZOffset = 0;
        int32_t vertexLabelsOffset = 0;
        int32_t faceVerticesOffset = 0;
        int32_t faceOrientationsOffset = 0;
        int32_t faceColorsOffset = 0;
        int32_t faceInfosOffset = 0;
        int32_t facePrioritiesOffset = 0;
        int32_t faceAlphasOffset = 0;
        int32_t faceLabelsOffset = 0;
        int32_t faceTextureAxisOffset = 0;
        bool loaded = false;
    };

    class Model : public Entity {
        friend class IfType;

    public:
        explicit Model(int32_t id);
        Model(int32_t count, const std::vector<std::shared_ptr<Model>>& models);
        Model(bool copyVertexY, bool copyFaces, Model& model);
        Model(bool shareColors, bool shareAlpha, bool shareVertices, Model& model);
        Model(int32_t count, int32_t dummy, const std::vector<std::shared_ptr<Model>>& models);
        Model() = default;
        void DrawSimple(int32_t pitch, int32_t yaw, int32_t roll, int32_t eyePitch, int32_t eyeX, int32_t eyeY, int32_t eyeZ);
        inline void DrawFace(int32_t face);
        void Draw(bool clipped, bool picking, int32_t bitset);
        void Draw(int32_t yaw, int32_t sinEyePitch, int32_t cosEyePitch, int32_t sinEyeYaw,
            int32_t cosEyeYaw, int32_t relativeX, int32_t relativeY, int32_t relativeZ, int32_t bitset) override;
        void DrawNearClippedFace(int32_t face);
        void CalculateNormals(int32_t lightAmbient, int32_t lightAttenuation, int32_t lightSrcX, int32_t lightSrcY, int32_t lightSrcZ, bool applyLighting);
        void CalculateBoundsCylinder();
        void CalculateBoundsY();
        void RotateY180();
        void RotateY90();
        void Recolor(int32_t src, int32_t dst);
        void Scale(int32_t x, int32_t y, int32_t z);
        void Translate(int32_t x, int32_t y, int32_t z);
        void CreateLabelReferences();
        void ApplyTransform(int32_t id);
        static void Init(int32_t count, OnDemand* onDemand);
        static void Unpack(const std::vector<int8_t>& src, int32_t id);
        static std::shared_ptr<Model> TryGet(int32_t id);
        void ApplyLighting(int32_t lightAmbient, int32_t lightAttenuation, int32_t lightSrcX, int32_t lightSrcY, int32_t lightSrcZ);
        void Set(Model& model, bool shareAlpha);
        void ApplyTransforms(int32_t primaryID, int32_t secondaryID, std::vector<int32_t>& mask);
        void ApplyTransform(int32_t type, const std::vector<int32_t>& labels, int32_t x, int32_t y, int32_t z);
        void RotateX(int32_t angle);
        static bool Validate(int32_t id);
        static void Unload();
        static void Unload(int32_t id);
    public:
        inline static int32_t mouseX = 0;
        inline static int32_t mouseY = 0;

        inline static int32_t pickedCount = 0;
        inline static std::array<int32_t, 1000> pickedBitsets{};

        inline static bool checkHover = false;

        int32_t radius = 0;
        std::vector<int32_t> vertexX;
        std::vector<int32_t> vertexY;
        std::vector<int32_t> vertexZ;
        int32_t vertexCount = 0;
        std::vector<std::vector<int32_t>> labelVertices;
        std::vector<std::vector<int32_t>> labelFaces;

        std::vector<VertexNormal> vertexNormalOriginal;

        /**
         * minDepth = (int) Math.sqrt((radius * radius) + (super.minY * super.minY));
         * maxDepth = minDepth + (int) Math.sqrt((radius * radius) + (maxY * maxY));
         */
        int32_t objRaise = 0;

        int32_t maxY = 0;
        int32_t minX = 0;
        int32_t maxX = 0;
        int32_t minZ = 0;
        int32_t maxZ = 0;
        int32_t faceCount = 0;

        std::vector<int32_t> faceVertexA;
        std::vector<int32_t> faceVertexB;
        std::vector<int32_t> faceVertexC;
        std::vector<int32_t> faceInfo;

        /**
         * When set to <code>true</code>, this model will be picked based on its projected screen bounds.
         *
         * @see #draw(int, int, int, int, int, int, int, int, int)
         */
        bool pickable = false;

    private:
        static bool PointWithinTriangle(int32_t x, int32_t y, int32_t yA, int32_t yB, int32_t yC, int32_t xA, int32_t xB, int32_t xC);
        static int32_t MulColorLightness(int32_t hsl, int32_t scalar, int32_t faceInfo);
        void CalculateBoundsAABB();
        int32_t AddVertex(Model& src, int32_t vertexId);

    private:
        static inline std::vector<Header> headers;
        static inline int32_t counter = 0;

        inline static int32_t baseX = 0;
        inline static int32_t baseY = 0;
        inline static int32_t baseZ = 0;

        int32_t texturedFaceCount = 0;
        int32_t priority = 0;
        int32_t maxDepth = 0;
        int32_t minDepth = 0;

        std::vector<int32_t> texturedVertexA;
        std::vector<int32_t> texturedVertexB;
        std::vector<int32_t> texturedVertexC;

        std::vector<int32_t> vertexLabel;
        std::vector<int32_t> facePriority;
        std::vector<int32_t> faceAlpha;
        std::vector<int32_t> faceLabel;
        std::vector<int32_t> faceColor;

        std::vector<int32_t> faceColorA;
        std::vector<int32_t> faceColorB;
        std::vector<int32_t> faceColorC;

        inline static std::array<int32_t, 4096> vertexScreenX{};
        inline static std::array<int32_t, 4096> vertexScreenY{};
        inline static std::array<int32_t, 4096> vertexScreenZ{};

        inline static std::array<int32_t, 4096> vertexViewSpaceX{};
        inline static std::array<int32_t, 4096> vertexViewSpaceY{};
        inline static std::array<int32_t, 4096> vertexViewSpaceZ{};

        inline static std::array<int32_t, 10> clippedX{};
        inline static std::array<int32_t, 10> clippedY{};
        inline static std::array<int32_t, 10> clippedColor{};

        inline static std::array<int32_t, 1500> tmpDepthFaceCount{};
        inline static std::array<int32_t, 12> tmpPriorityFaceCount{};
        inline static std::array<int32_t, 12> tmpPriorityDepthSum{};

        inline static std::array<uint8_t, 4096> faceClippedX{};
        inline static std::array<uint8_t, 4096> faceNearClipped{};

        inline static Array2DIndexed<int32_t> tmpDepthFaces = Array2DIndexed<int32_t>(1500, 512);
        inline static std::vector<std::vector<int32_t>> tmpPriorityFaces = std::vector(12, std::vector(2000, 0));

        inline static std::array<int32_t, 2000> tmpPriority10FaceDepth{};
        inline static std::array<int32_t, 2000> tmpPriority11FaceDepth{};

        inline static std::vector<int32_t> tmpVertexX = std::vector<int32_t>(2000);
        inline static std::vector<int32_t> tmpVertexY = std::vector<int32_t>(2000);
        inline static std::vector<int32_t> tmpVertexZ = std::vector<int32_t>(2000);
        inline static std::vector<int32_t> tmpFaceAlpha = std::vector<int32_t>(2000);

        static inline OnDemand* ondemand = nullptr;
    };

}
