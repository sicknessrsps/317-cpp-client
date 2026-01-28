#include "SceneTile.h"

namespace SDL_Client {

    const JaggedArray<int8_t> SceneTileOverlay::SHAPE_POINTS = JaggedArray<int8_t>({
        {1, 3, 5, 7},
        {1, 3, 5, 7},
        {1, 3, 5, 7},
        {1, 3, 5, 7, 6},
        {1, 3, 5, 7, 6},
        {1, 3, 5, 7, 6},
        {1, 3, 5, 7, 6},
        {1, 3, 5, 7, 2, 6},
        {1, 3, 5, 7, 2, 8},
        {1, 3, 5, 7, 2, 8},
        {1, 3, 5, 7, 11, 12},
        {1, 3, 5, 7, 11, 12},
        {1, 3, 5, 7, 13, 14},
    });


    const JaggedArray<int8_t> SceneTileOverlay::SHAPE_PATHS = JaggedArray<int8_t>({
     {0, 1, 2, 3, 0, 0, 1, 3},
     {1, 1, 2, 3, 1, 0, 1, 3},
     {0, 1, 2, 3, 1, 0, 1, 3},
     {0, 0, 1, 2, 0, 0, 2, 4, 1, 0, 4, 3},
     {0, 0, 1, 4, 0, 0, 4, 3, 1, 1, 2, 4},
     {0, 0, 4, 3, 1, 0, 1, 2, 1, 0, 2, 4},
     {0, 1, 2, 4, 1, 0, 1, 4, 1, 0, 4, 3},
     {0, 4, 1, 2, 0, 4, 2, 5, 1, 0, 4, 5, 1, 0, 5, 3},
     {0, 4, 1, 2, 0, 4, 2, 3, 0, 4, 3, 5, 1, 0, 4, 5},
     {0, 0, 4, 5, 1, 4, 1, 2, 1, 4, 2, 3, 1, 4, 3, 5},
     {0, 0, 1, 5, 0, 1, 4, 5, 0, 1, 2, 4, 1, 0, 5, 3, 1, 5, 4, 3, 1, 4, 2, 3},
     {1, 0, 1, 5, 1, 1, 4, 5, 1, 1, 2, 4, 0, 0, 5, 3, 0, 5, 4, 3, 0, 4, 2, 3},
     {1, 0, 5, 4, 1, 0, 1, 5, 0, 0, 4, 3, 0, 4, 5, 3, 0, 5, 2, 3, 0, 1, 2, 5},
    });

    std::vector<int32_t> SceneTileOverlay::tmpScreenX(6);
    std::vector<int32_t> SceneTileOverlay::tmpScreenY(6);
    std::vector<int32_t> SceneTileOverlay::tmpViewspaceX(6);
    std::vector<int32_t> SceneTileOverlay::tmpViewspaceY(6);
    std::vector<int32_t> SceneTileOverlay::tmpViewspaceZ(6);

    SceneTile::SceneTile(int32_t level, int32_t x, int32_t z)
        : overlay(), occludeLevel(level), level(level), x(x), z(z), initialized(true)
    {
    }

    SceneTileOverlay::SceneTileOverlay(int32_t tileZ, int32_t southwestColor2, int32_t northwestColor1,
                                       int32_t northeastY, int32_t textureID, int32_t northeastColor2, int32_t rotation, int32_t southwestColor1,
                                       int32_t backgroundRGB, int32_t northeastColor1, int32_t northwestY, int32_t southeastY, int32_t southwestY,
                                       int32_t shape, int32_t northwestColor2, int32_t southeastColor2, int32_t southeastColor1, int32_t tileX,
                                       int32_t foregroundRGB)
        : flat((southwestY == southeastY) && (southwestY == northeastY) && (southwestY == northwestY)),
          shape(shape),
          rotation(rotation),
          backgroundRGB(backgroundRGB),
          foregroundRGB(foregroundRGB),
          initialized(true)
    {
        constexpr int32_t ONE = 128;
        constexpr int32_t HALF = ONE / 2;
        constexpr int32_t QUARTER = ONE / 4;
        constexpr int32_t THREE_QUARTER = (ONE * 3) / 4;

        const auto& points = SHAPE_POINTS[shape];
        size_t vertexCount = points.size();

        vertexX.resize(vertexCount);
        vertexY.resize(vertexCount);
        vertexZ.resize(vertexCount);

        std::vector<int32_t> primaryColors(vertexCount);
        std::vector<int32_t> secondaryColors(vertexCount);

        int32_t sceneX = tileX * ONE;
        int32_t sceneZ = tileZ * ONE;

        for (int32_t v = 0; v < vertexCount; v++) {
            int32_t type = points[v];

            if (((type & 1) == 0) && (type <= 8)) {
                type = ((type - rotation - rotation - 1) & 7) + 1;
            }

            if ((type > 8) && (type <= 12)) {
                type = ((type - 9 - rotation) & 3) + 9;
            }

            if ((type > 12) && (type <= 16)) {
                type = ((type - 13 - rotation) & 3) + 13;
            }

            int32_t x;
            int32_t z;
            int32_t y;
            int32_t color1;
            int32_t color2;
            if (type == 1) {
                x = sceneX;
                z = sceneZ;
                y = southwestY;
                color1 = southwestColor1;
                color2 = southwestColor2;
            } else if (type == 2) {
                x = sceneX + HALF;
                z = sceneZ;
                y = (southwestY + southeastY) >> 1;
                color1 = (southwestColor1 + southeastColor1) >> 1;
                color2 = (southwestColor2 + southeastColor2) >> 1;
            } else if (type == 3) {
                x = sceneX + ONE;
                z = sceneZ;
                y = southeastY;
                color1 = southeastColor1;
                color2 = southeastColor2;
            } else if (type == 4) {
                x = sceneX + ONE;
                z = sceneZ + HALF;
                y = (southeastY + northeastY) >> 1;
                color1 = (southeastColor1 + northeastColor1) >> 1;
                color2 = (southeastColor2 + northeastColor2) >> 1;
            } else if (type == 5) {
                x = sceneX + ONE;
                z = sceneZ + ONE;
                y = northeastY;
                color1 = northeastColor1;
                color2 = northeastColor2;
            } else if (type == 6) {
                x = sceneX + HALF;
                z = sceneZ + ONE;
                y = (northeastY + northwestY) >> 1;
                color1 = (northeastColor1 + northwestColor1) >> 1;
                color2 = (northeastColor2 + northwestColor2) >> 1;
            } else if (type == 7) {
                x = sceneX;
                z = sceneZ + ONE;
                y = northwestY;
                color1 = northwestColor1;
                color2 = northwestColor2;
            } else if (type == 8) {
                x = sceneX;
                z = sceneZ + HALF;
                y = (northwestY + southwestY) >> 1;
                color1 = (northwestColor1 + southwestColor1) >> 1;
                color2 = (northwestColor2 + southwestColor2) >> 1;
            } else if (type == 9) {
                x = sceneX + HALF;
                z = sceneZ + QUARTER;
                y = (southwestY + southeastY) >> 1;
                color1 = (southwestColor1 + southeastColor1) >> 1;
                color2 = (southwestColor2 + southeastColor2) >> 1;
            } else if (type == 10) {
                x = sceneX + THREE_QUARTER;
                z = sceneZ + HALF;
                y = (southeastY + northeastY) >> 1;
                color1 = (southeastColor1 + northeastColor1) >> 1;
                color2 = (southeastColor2 + northeastColor2) >> 1;
            } else if (type == 11) {
                x = sceneX + HALF;
                z = sceneZ + THREE_QUARTER;
                y = (northeastY + northwestY) >> 1;
                color1 = (northeastColor1 + northwestColor1) >> 1;
                color2 = (northeastColor2 + northwestColor2) >> 1;
            } else if (type == 12) {
                x = sceneX + QUARTER;
                z = sceneZ + HALF;
                y = (northwestY + southwestY) >> 1;
                color1 = (northwestColor1 + southwestColor1) >> 1;
                color2 = (northwestColor2 + southwestColor2) >> 1;
            } else if (type == 13) {
                x = sceneX + QUARTER;
                z = sceneZ + QUARTER;
                y = southwestY;
                color1 = southwestColor1;
                color2 = southwestColor2;
            } else if (type == 14) {
                x = sceneX + THREE_QUARTER;
                z = sceneZ + QUARTER;
                y = southeastY;
                color1 = southeastColor1;
                color2 = southeastColor2;
            } else if (type == 15) {
                x = sceneX + THREE_QUARTER;
                z = sceneZ + THREE_QUARTER;
                y = northeastY;
                color1 = northeastColor1;
                color2 = northeastColor2;
            } else {
                x = sceneX + QUARTER;
                z = sceneZ + THREE_QUARTER;
                y = northwestY;
                color1 = northwestColor1;
                color2 = northwestColor2;
            }
            vertexX[v] = x;
            vertexY[v] = y;
            vertexZ[v] = z;
            primaryColors[v] = color1;
            secondaryColors[v] = color2;
        }

        const auto& paths = SHAPE_PATHS[shape];
        size_t triangleCount = paths.size() / 4;
        triangleVertexA.resize(triangleCount);
        triangleVertexB.resize(triangleCount);
        triangleVertexC.resize(triangleCount);
        triangleColorA.resize(triangleCount);
        triangleColorB.resize(triangleCount);
        triangleColorC.resize(triangleCount);

        if (textureID != -1) {
            triangleTextureIDs.resize(triangleCount);
        }

        int32_t index = 0;
        for (int32_t i = 0; i < triangleCount; i++) {
            int32_t color = paths[index];
            int32_t a = paths[index + 1];
            int32_t b = paths[index + 2];
            int32_t c = paths[index + 3];

            index += 4;

            if (a < 4) {
                a = (a - rotation) & 3;
            }

            if (b < 4) {
                b = (b - rotation) & 3;
            }

            if (c < 4) {
                c = (c - rotation) & 3;
            }

            triangleVertexA[i] = a;
            triangleVertexB[i] = b;
            triangleVertexC[i] = c;

            if (color == 0) {
                triangleColorA[i] = primaryColors[a];
                triangleColorB[i] = primaryColors[b];
                triangleColorC[i] = primaryColors[c];
                if (!triangleTextureIDs.empty()) {
                    triangleTextureIDs[i] = -1;
                }
            } else {
                triangleColorA[i] = secondaryColors[a];
                triangleColorB[i] = secondaryColors[b];
                triangleColorC[i] = secondaryColors[c];
                if (!triangleTextureIDs.empty()) {
                    triangleTextureIDs[i] = textureID;
                }
            }
        }
    }
}
