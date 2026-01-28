#include "Model.h"
#include "Buffer.h"
#include "Draw3D.h"
#include "Draw2D.h"
#include "SeqTransform.h"

namespace SDL_Client {

    /**
     * Constructs a new model and loads it from its header.
     *
     * <b>Note:</b> This constructor only works if the model has already been loaded. {@link #Unpack(std::vector<int8_t> &src, int32_t)} must
     * have already been called for the given <code>id</code>.
     *
     * @param id the model id.
     */
    Model::Model(const int32_t id) {

        counter++;

        const Header& header = headers[id];
        vertexCount = header.vertexCount;
        faceCount = header.faceCount;
        texturedFaceCount = header.texturedFaceCount;

        vertexX.resize(vertexCount);
        vertexY.resize(vertexCount);
        vertexZ.resize(vertexCount);

        faceVertexA.resize(faceCount);
        faceVertexB.resize(faceCount);
        faceVertexC.resize(faceCount);

        texturedVertexA.resize(texturedFaceCount);
        texturedVertexB.resize(texturedFaceCount);
        texturedVertexC.resize(texturedFaceCount);

        if (header.vertexLabelsOffset >= 0) {
            vertexLabel.resize(vertexCount);
        }

        if (header.faceInfosOffset >= 0) {
            faceInfo.resize(faceCount);
        }

        if (header.facePrioritiesOffset >= 0) {
            facePriority.resize(faceCount);
        } else {
            priority = -header.facePrioritiesOffset - 1;
        }

        if (header.faceAlphasOffset >= 0) {
            faceAlpha.resize(faceCount);
        }

        if (header.faceLabelsOffset >= 0) {
            faceLabel.resize(faceCount);
        }

        faceColor.resize(faceCount);

        Buffer buf0(header.data);
        buf0.position = header.vertexFlagsOffset;

        Buffer buf1(header.data);
        buf1.position = header.vertexXOffset;

        Buffer buf2(header.data);
        buf2.position = header.vertexYOffset;

        Buffer buf3(header.data);
        buf3.position = header.vertexZOffset;

        Buffer buf4(header.data);
        buf4.position = header.vertexLabelsOffset;

        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;

        //printf("vertex count: %i\n", vertexCount);

        for (int32_t v = 0; v < vertexCount; v++) {
            const int32_t flags = buf0.ReadU8();

            int32_t dx = 0;
            int32_t dy = 0;
            int32_t dz = 0;

            if ((flags & 1) != 0) {
                dx = buf1.ReadSmart();
            }

            if ((flags & 2) != 0) {
                dy = buf2.ReadSmart();
            }

            if ((flags & 4) != 0) {
                dz = buf3.ReadSmart();
            }

            vertexX[v] = x + dx;
            vertexY[v] = y + dy;
            vertexZ[v] = z + dz;

            x = vertexX[v];
            y = vertexY[v];
            z = vertexZ[v];

            if (!vertexLabel.empty()) {
                vertexLabel[v] = buf4.ReadU8();
            }
        }

        buf0.position = header.faceColorsOffset;
        buf1.position = header.faceInfosOffset;
        buf2.position = header.facePrioritiesOffset;
        buf3.position = header.faceAlphasOffset;
        buf4.position = header.faceLabelsOffset;

        for (int32_t face = 0; face < faceCount; face++) {
            faceColor[face] = buf0.ReadU16();

            if (!faceInfo.empty()) {
                faceInfo[face] = buf1.ReadU8();
            }

            if (!facePriority.empty()) {
                facePriority[face] = buf2.ReadU8();
            }

            if (!faceAlpha.empty()) {
                faceAlpha[face] = buf3.ReadU8();
            }

            if (!faceLabel.empty()) {
                faceLabel[face] = buf4.ReadU8();
            }
        }

        buf0.position = header.faceVerticesOffset;
        buf1.position = header.faceOrientationsOffset;

        int32_t a = 0;
        int32_t b = 0;
        int32_t c = 0;
        int32_t last = 0;

        for (int32_t face = 0; face < faceCount; face++) {
            const int32_t orientation = buf1.ReadU8();

            // fancy shmansy compression type stuff.
            // vertex indices stored as deltas, with some faces
            // sharing indices.

            // new a, b, c
            if (orientation == 1) {
                a = buf0.ReadSmart() + last;
                last = a;
                b = buf0.ReadSmart() + last;
                last = b;
                c = buf0.ReadSmart() + last;
                last = c;
                faceVertexA[face] = a;
                faceVertexB[face] = b;
                faceVertexC[face] = c;
            }

            // reuse a, c, new b
            if (orientation == 2) {
                b = c;
                c = buf0.ReadSmart() + last;
                last = c;
                faceVertexA[face] = a;
                faceVertexB[face] = b;
                faceVertexC[face] = c;
            }

            // reuse c, b, new a
            if (orientation == 3) {
                a = c;
                c = buf0.ReadSmart() + last;
                last = c;
                faceVertexA[face] = a;
                faceVertexB[face] = b;
                faceVertexC[face] = c;
            }

            // reuse b, a, new c
            if (orientation == 4) {
                const int32_t tmp = a;
                a = b;
                b = tmp;
                c = buf0.ReadSmart() + last;
                last = c;
                faceVertexA[face] = a;
                faceVertexB[face] = b;
                faceVertexC[face] = c;
            }
        }

        buf0.position = header.faceTextureAxisOffset;

        for (int32_t face = 0; face < texturedFaceCount; face++) {
            texturedVertexA[face] = buf0.ReadU16();
            texturedVertexB[face] = buf0.ReadU16();
            texturedVertexC[face] = buf0.ReadU16();
        }
    }

    /**
     * Constructs a new model by merging the provided models. This constructor is used to combine models <i>before</i>
     * {@link SeqTransform}'s have been applied. Using this constructor implies all models merged are compatible with any
     * animations played on them.
     *
     * @param count  the model count.
     * @param models the models to merge.
     */
    Model::Model(int32_t count, const std::vector<std::shared_ptr<Model>>& models)
    {
        counter++;

        bool copyInfo = false;
        bool copyPriority = false;
        bool copyAlpha = false;
        bool copyLabels = false;

        vertexCount = 0;
        faceCount = 0;
        texturedFaceCount = 0;
        priority = -1;

        for (int32_t i = 0; i < count; i++) {
            const auto& model = models[i];

            if (model == nullptr) {
                continue;
            }

            vertexCount += model->vertexCount;
            faceCount += model->faceCount;
            texturedFaceCount += model->texturedFaceCount;

            copyInfo |= !model->faceInfo.empty();

            if (!model->facePriority.empty()) {
                copyPriority = true;
            } else {
                if (priority == -1) {
                    priority = model->priority;
                }
                if (priority != model->priority) {
                    copyPriority = true;
                }
            }

            copyAlpha |= !model->faceAlpha.empty();
            copyLabels |= !model->faceLabel.empty();
        }

        vertexX.resize(vertexCount);
        vertexY.resize(vertexCount);
        vertexZ.resize(vertexCount);
        vertexLabel.resize(vertexCount);
        faceVertexA.resize(faceCount);
        faceVertexB.resize(faceCount);
        faceVertexC.resize(faceCount);
        texturedVertexA.resize(texturedFaceCount);
        texturedVertexB.resize(texturedFaceCount);
        texturedVertexC.resize(texturedFaceCount);

        if (copyInfo) {
            faceInfo.resize(faceCount);
        }

        if (copyPriority) {
            facePriority.resize(faceCount);
        }

        if (copyAlpha) {
            faceAlpha.resize(faceCount);
        }

        if (copyLabels) {
            faceLabel.resize(faceCount);
        }

        faceColor.resize(faceCount);
        vertexCount = 0;
        faceCount = 0;
        texturedFaceCount = 0;

        int32_t tfaceCount = 0;

        for (int32_t i = 0; i < count; i++)
        {
            const auto& model = models[i];

            if (model == nullptr) {
                continue;
            }

            for (int32_t face = 0; face < model->faceCount; face++) {
                if (copyInfo) {
                    if (model->faceInfo.empty()) {
                        faceInfo[faceCount] = 0;
                    } else {
                        int32_t info = model->faceInfo[face];

                        if ((info & 2) == 2) {
                            info += tfaceCount << 2;
                        }

                        faceInfo[faceCount] = info;
                    }
                }

                if (copyPriority) {
                    if (model->facePriority.empty()) {
                        facePriority[faceCount] = model->priority;
                    } else {
                        facePriority[faceCount] = model->facePriority[face];
                    }
                }

                if (copyAlpha) {
                    if (model->faceAlpha.empty()) {
                        faceAlpha[faceCount] = 0;
                    } else {
                        faceAlpha[faceCount] = model->faceAlpha[face];
                    }
                }

                if (copyLabels && (!model->faceLabel.empty())) {
                    faceLabel[faceCount] = model->faceLabel[face];
                }

                faceColor[faceCount] = model->faceColor[face];
                faceVertexA[faceCount] = AddVertex(*model, model->faceVertexA[face]);
                faceVertexB[faceCount] = AddVertex(*model, model->faceVertexB[face]);
                faceVertexC[faceCount] = AddVertex(*model, model->faceVertexC[face]);
                faceCount++;
            }

            for (int32_t face = 0; face < model->texturedFaceCount; face++) {
                texturedVertexA[texturedFaceCount] = AddVertex(*model, model->texturedVertexA[face]);
                texturedVertexB[texturedFaceCount] = AddVertex(*model, model->texturedVertexB[face]);
                texturedVertexC[texturedFaceCount] = AddVertex(*model, model->texturedVertexC[face]);
                texturedFaceCount++;
            }

            tfaceCount += model->texturedFaceCount;
        }
    }



    /**
     * Constructs a new model with the options to copy either vertex Y positions and faces.
     * <p>
     * This constructor is specifically used by Locs which adjust to terrain.
     *
     * @param copyVertexY <code>true</code> to copy <code>model.vertexY</code>.
     * @param copyFaces   <code>true</code> to copy all face data from <code>model</code>.
     * @param model       the model to copy.
     * @see LocType#getModel(int, int, int, int, int, int, int)
     */
    Model::Model(bool copyVertexY, bool copyFaces, Model& model)
    {
        counter++;
        vertexCount = model.vertexCount;
        faceCount = model.faceCount;
        texturedFaceCount = model.texturedFaceCount;

        if (copyVertexY) {
            vertexY.resize(vertexCount);
            for (int32_t v = 0; v < vertexCount; v++) {
                std::copy_n(model.vertexY.begin(), vertexCount, vertexY.begin());
            }
        } else {
            vertexY = model.vertexY;
        }

        if (copyFaces) {
            faceColorA = std::vector<int32_t>(faceCount);
            faceColorB = std::vector<int32_t>(faceCount);
            faceColorC = std::vector<int32_t>(faceCount);

            std::copy_n(model.faceColorA.begin(), faceCount, faceColorA.begin());
            std::copy_n(model.faceColorB.begin(), faceCount, faceColorB.begin());
            std::copy_n(model.faceColorC.begin(), faceCount, faceColorC.begin());

            faceInfo.resize(faceCount);

            if (model.faceInfo.empty()) {
                for (int l = 0; l < faceCount; l++) {
                    faceInfo[l] = 0;
                }
            } else {
                std::copy_n(model.faceInfo.begin(), faceCount, faceInfo.begin());
            }

            vertexNormal.resize(vertexCount);
            std::copy_n(model.vertexNormal.begin(), vertexCount, vertexNormal.begin());
            vertexNormalOriginal = model.vertexNormalOriginal;
        } else {
            faceColorA = model.faceColorA;
            faceColorB = model.faceColorB;
            faceColorC = model.faceColorC;
            faceInfo = model.faceInfo;
        }

        vertexX = model.vertexX;
        vertexZ = model.vertexZ;
        faceColor = model.faceColor;
        faceAlpha = model.faceAlpha;
        facePriority = model.facePriority;
        priority = model.priority;
        faceVertexA = model.faceVertexA;
        faceVertexB = model.faceVertexB;
        faceVertexC = model.faceVertexC;
        texturedVertexA = model.texturedVertexA;
        texturedVertexB = model.texturedVertexB;
        texturedVertexC = model.texturedVertexC;
        minY = model.minY;
        maxY = model.maxY;
        radius = model.radius;
        minDepth = model.minDepth;
        maxDepth = model.maxDepth;
        minX = model.minX;
        maxZ = model.maxZ;
        minZ = model.minZ;
        maxX = model.maxX;
    }

    /**
     * Constructs a new model that can either share or clone the provided models attributes. This constructor is used to
     * save memory by avoiding allocations, and is meant to be used with models prior to having a {@link SeqTransform} applied.
     *
     * @param shareColors   <code>true</code> to reference the provided model's colors.
     * @param shareAlpha    <code>true</code> to reference the provided model's face alpha.
     * @param shareVertices <code>true</code> to reference the provided model's vertices.
     * @param model         the model to share or clone.
     */
    Model::Model(bool shareColors, bool shareAlpha, bool shareVertices, Model& model)
    {
        counter++;
        vertexCount = model.vertexCount;
        faceCount = model.faceCount;
        texturedFaceCount = model.texturedFaceCount;

        if (shareVertices) {
            vertexX = model.vertexX;
            vertexY = model.vertexY;
            vertexZ = model.vertexZ;
        } else {
            vertexX.resize(vertexCount);
            vertexY.resize(vertexCount);
            vertexZ.resize(vertexCount);

            for (int32_t j = 0; j < vertexCount; j++) {
                vertexX[j] = model.vertexX[j];
                vertexY[j] = model.vertexY[j];
                vertexZ[j] = model.vertexZ[j];
            }
        }

        if (shareColors) {
            faceColor = model.faceColor;
        } else {
            faceColor.resize(faceCount);

            for (int32_t k = 0; k < faceCount; k++) {
                faceColor[k] = model.faceColor[k];
            }
        }

        if (shareAlpha) {
            faceAlpha = model.faceAlpha;
        } else {
            faceAlpha.resize(faceCount);
            if (model.faceAlpha.empty()) {
                for (int32_t l = 0; l < faceCount; l++) {
                    faceAlpha[l] = 0;
                }
            } else {
                for (int32_t i1 = 0; i1 < faceCount; i1++) {
                    faceAlpha[i1] = model.faceAlpha[i1];
                }
            }
        }

        vertexLabel = model.vertexLabel;
        faceLabel = model.faceLabel;
        faceInfo = model.faceInfo;
        faceVertexA = model.faceVertexA;
        faceVertexB = model.faceVertexB;
        faceVertexC = model.faceVertexC;
        facePriority = model.facePriority;
        priority = model.priority;
        texturedVertexA = model.texturedVertexA;
        texturedVertexB = model.texturedVertexB;
        texturedVertexC = model.texturedVertexC;
    }

    /**
     * Constructs a new model by combining models. This constructor is commonly used to combine models which have already
     * had a {@link SeqTransform} applied, and does not copy label information.
     *
     * @param count  the model count.
     * @param dummy  dummy.
     * @param models the models.
     */
    Model::Model(int32_t count, int32_t dummy, const std::vector<std::shared_ptr<Model>>& models)
    {
        counter++;

        bool copyInfo = false;
        bool copyPriority = false;
        bool copyAlpha = false;
        bool copyColor = false;

        vertexCount = 0;
        faceCount = 0;
        texturedFaceCount = 0;
        priority = -1;

        for (int32_t i = 0; i < count; i++) {
            const auto& model = models[i];

            if (model == nullptr) {
                continue;
            }

            vertexCount += model->vertexCount;
            faceCount += model->faceCount;
            texturedFaceCount += model->texturedFaceCount;
            copyInfo |= !model->faceInfo.empty();

            if (!model->facePriority.empty()) {
                copyPriority = true;
            } else {
                if (priority == -1) {
                    priority = model->priority;
                }
                if (priority != model->priority) {
                    copyPriority = true;
                }
            }

            copyAlpha |= !model->faceAlpha.empty();
            copyColor |= !model->faceColor.empty();
        }

        vertexX.resize(vertexCount);
        vertexY.resize(vertexCount);
        vertexZ.resize(vertexCount);
        faceVertexA.resize(faceCount);
        faceVertexB.resize(faceCount);
        faceVertexC.resize(faceCount);
        faceColorA.resize(faceCount);
        faceColorB.resize(faceCount);
        faceColorC.resize(faceCount);
        texturedVertexA.resize(texturedFaceCount);
        texturedVertexB.resize(texturedFaceCount);
        texturedVertexC.resize(texturedFaceCount);

        if (copyInfo) {
            faceInfo.resize(faceCount);
        }

        if (copyPriority) {
            facePriority.resize(faceCount);
        }

        if (copyAlpha) {
            faceAlpha.resize(faceCount);
        }

        if (copyColor) {
            faceColor.resize(faceCount);
        }

        vertexCount = 0;
        faceCount = 0;
        texturedFaceCount = 0;

        int32_t tfaceCount = 0;
        for (int32_t i = 0; i < count; i++) {
            const auto& model = models[i];

            if (model == nullptr) {
                continue;
            }

            int32_t baseVertexCount = this->vertexCount;

            for (int32_t v = 0; v < model->vertexCount; v++) {
                vertexX[this->vertexCount] = model->vertexX[v];
                vertexY[this->vertexCount] = model->vertexY[v];
                vertexZ[this->vertexCount] = model->vertexZ[v];
                this->vertexCount++;
            }

            for (int32_t f = 0; f < model->faceCount; f++) {
                faceVertexA[faceCount] = model->faceVertexA[f] + baseVertexCount;
                faceVertexB[faceCount] = model->faceVertexB[f] + baseVertexCount;
                faceVertexC[faceCount] = model->faceVertexC[f] + baseVertexCount;

                faceColorA[faceCount] = model->faceColorA[f];
                faceColorB[faceCount] = model->faceColorB[f];
                faceColorC[faceCount] = model->faceColorC[f];

                if (copyInfo) {
                    if (model->faceInfo.empty()) {
                        faceInfo[faceCount] = 0;
                    } else {
                        int32_t info = model->faceInfo[f];

                        if ((info & 2) == 2) {
                            info += tfaceCount << 2;
                        }

                        faceInfo[faceCount] = info;
                    }
                }

                if (copyPriority) {
                    if (model->facePriority.empty()) {
                        facePriority[faceCount] = model->priority;
                    } else {
                        facePriority[faceCount] = model->facePriority[f];
                    }
                }

                if (copyAlpha) {
                    if (model->faceAlpha.empty()) {
                        faceAlpha[faceCount] = 0;
                    } else {
                        faceAlpha[faceCount] = model->faceAlpha[f];
                    }
                }

                if (copyColor && (!model->faceColor.empty())) {
                    faceColor[faceCount] = model->faceColor[f];
                }
                faceCount++;
            }

            for (int32_t f = 0; f < model->texturedFaceCount; f++) {
                texturedVertexA[texturedFaceCount] = model->texturedVertexA[f] + baseVertexCount;
                texturedVertexB[texturedFaceCount] = model->texturedVertexB[f] + baseVertexCount;
                texturedVertexC[texturedFaceCount] = model->texturedVertexC[f] + baseVertexCount;
                texturedFaceCount++;
            }

            tfaceCount += model->texturedFaceCount;
        }
        CalculateBoundsCylinder();
    }

    /**
     * Draws this model with reduced parameters. Call CalculateNormals before.
     *
     * @param pitch    the model pitch.
     * @param yaw      the model yaw.
     * @param roll     the model roll.
     * @param eyePitch the eye pitch.
     * @param eyeX     the eye x.
     * @param eyeY     the eye y.
     * @param eyeZ     the eye z.
     */
    void Model::DrawSimple(int32_t pitch, int32_t yaw, int32_t roll, int32_t eyePitch, int32_t eyeX, int32_t eyeY,
        int32_t eyeZ) {
        int32_t centerX = Draw3D::centerX;
        int32_t centerY = Draw3D::centerY;
        int32_t sinPitch = Draw3D::sin[pitch];
        int32_t cosPitch = Draw3D::cos[pitch];
        int32_t sinYaw = Draw3D::sin[yaw];
        int32_t cosYaw = Draw3D::cos[yaw];
        int32_t sinRoll = Draw3D::sin[roll];
        int32_t cosRoll = Draw3D::cos[roll];
        int32_t sinEyePitch = Draw3D::sin[eyePitch];
        int32_t cosEyePitch = Draw3D::cos[eyePitch];
        int32_t midZ = ((eyeY * sinEyePitch) + (eyeZ * cosEyePitch)) >> 16;

        for (int32_t v = 0; v < vertexCount; v++) {
            int32_t x = vertexX[v];
            int32_t y = vertexY[v];
            int32_t z = vertexZ[v];

            // Local Space -> Model Space

            if (roll != 0) {
                int32_t x_ = ((y * sinRoll) + (x * cosRoll)) >> 16;
                y = ((y * cosRoll) - (x * sinRoll)) >> 16;
                x = x_;
            }

            if (pitch != 0) {
                int32_t y_ = ((y * cosPitch) - (z * sinPitch)) >> 16;
                z = ((y * sinPitch) + (z * cosPitch)) >> 16;
                y = y_;
            }

            if (yaw != 0) {
                int32_t x_ = ((z * sinYaw) + (x * cosYaw)) >> 16;
                z = ((z * cosYaw) - (x * sinYaw)) >> 16;
                x = x_;
            }

            // Model Space -> View Space

            x += eyeX;
            y += eyeY;
            z += eyeZ;

            int32_t y_ = ((y * cosEyePitch) - (z * sinEyePitch)) >> 16;
            z = ((y * sinEyePitch) + (z * cosEyePitch)) >> 16;
            y = y_;

            // View Space -> Screen Space

            vertexScreenX[v] = centerX + ((x << 9) / z);
            vertexScreenY[v] = centerY + ((y << 9) / z);
            vertexScreenZ[v] = z - midZ;

            // Store viewspace coordinates to be transformed into screen space later (textured or clipped triangles)

            if (texturedFaceCount > 0) {
                vertexViewSpaceX[v] = x;
                vertexViewSpaceY[v] = y;
                vertexViewSpaceZ[v] = z;
            }
        }
        Draw(false, false, 0);
    }

    /**
     * Draws the provided face id.
     *
     * @param face the face id.
     */
    void Model::DrawFace(int32_t face) {
        if (faceNearClipped[face]) {
            DrawNearClippedFace(face);
            return;
        }

        const int32_t a = faceVertexA[face];
        const int32_t b = faceVertexB[face];
        const int32_t c = faceVertexC[face];
        Draw3D::clipX = faceClippedX[face];

        if (faceAlpha.empty()) {
            Draw3D::alpha = 0;
        } else {
            Draw3D::alpha = faceAlpha[face];
        }

        int32_t type;

        if (faceInfo.empty()) {
            type = 0;
        } else {
            type = faceInfo[face] & 0b11;
        }

        // Cache vertex screen coordinates
        const int32_t screenYA = vertexScreenY[a];
        const int32_t screenYB = vertexScreenY[b];
        const int32_t screenYC = vertexScreenY[c];
        const int32_t screenXA = vertexScreenX[a];
        const int32_t screenXB = vertexScreenX[b];
        const int32_t screenXC = vertexScreenX[c];

        if (type == 0) {
            Draw3D::FillGouraudTriangle(screenYA, screenYB, screenYC, screenXA, screenXB, screenXC, faceColorA[face], faceColorB[face], faceColorC[face]);
        } else if (type == 1) {
            Draw3D::FillTriangle(screenYA, screenYB, screenYC, screenXA, screenXB, screenXC, Draw3D::palette[faceColorA[face]]);
        } else if (type == 2) {
            int32_t texturedFace = faceInfo[face] >> 2;
            int32_t ta = texturedVertexA[texturedFace];
            int32_t tb = texturedVertexB[texturedFace];
            int32_t tc = texturedVertexC[texturedFace];
            Draw3D::FillTexturedTriangle(screenYA, screenYB, screenYC, screenXA, screenXB, screenXC, faceColorA[face], faceColorB[face], faceColorC[face], vertexViewSpaceX[ta], vertexViewSpaceX[tb], vertexViewSpaceX[tc], vertexViewSpaceY[ta], vertexViewSpaceY[tb], vertexViewSpaceY[tc], vertexViewSpaceZ[ta], vertexViewSpaceZ[tb], vertexViewSpaceZ[tc], faceColor[face]);
        } else if (type == 3) {
            int32_t texturedFace = faceInfo[face] >> 2;
            int32_t ta = texturedVertexA[texturedFace];
            int32_t tb = texturedVertexB[texturedFace];
            int32_t tc = texturedVertexC[texturedFace];
            const int32_t color = faceColorA[face];
            Draw3D::FillTexturedTriangle(screenYA, screenYB, screenYC, screenXA, screenXB, screenXC, color, color, color, vertexViewSpaceX[ta], vertexViewSpaceX[tb], vertexViewSpaceX[tc], vertexViewSpaceY[ta], vertexViewSpaceY[tb], vertexViewSpaceY[tc], vertexViewSpaceZ[ta], vertexViewSpaceZ[tb], vertexViewSpaceZ[tc], faceColor[face]);
        }
    }

    /**
     * Draws the {@link Model} with the provided parameters. This method performs depth and priority sorting.
     *
     * @param clipped whether to check near plane clipping.
     * @param picking <code>true</code> to enable picking.
     * @param bitset  the bitset. Used with <code>pick</code> set to true.
     */
    void Model::Draw(bool clipped, bool picking, int32_t bitset) {
        // Clear depth face counts - use memset for better performance
        std::fill(tmpDepthFaceCount.begin(), tmpDepthFaceCount.begin() + maxDepth, 0);

        // Cache pointer to avoid repeated bounds checking
        const int32_t* faceVertexAPtr = faceVertexA.data();
        const int32_t* faceVertexBPtr = faceVertexB.data();
        const int32_t* faceVertexCPtr = faceVertexC.data();
        const int32_t* vertexScreenXPtr = vertexScreenX.data();
        const int32_t* vertexScreenYPtr = vertexScreenY.data();
        const int32_t* vertexScreenZPtr = vertexScreenZ.data();
        const bool hasFaceInfo = !faceInfo.empty();
        const int32_t* faceInfoPtr = hasFaceInfo ? faceInfo.data() : nullptr;
        const int32_t boundXCached = Draw2D::boundX;

        for (int32_t f = 0; f < faceCount; f++) {
            if (hasFaceInfo && (faceInfoPtr[f] == -1)) {
                continue;
            }

            const int32_t a = faceVertexAPtr[f];
            const int32_t b = faceVertexBPtr[f];
            const int32_t c = faceVertexCPtr[f];
            const int32_t xA = vertexScreenXPtr[a];
            const int32_t xB = vertexScreenXPtr[b];
            const int32_t xC = vertexScreenXPtr[c];

            if (clipped && ((xA == -5000) || (xB == -5000) || (xC == -5000))) {
                faceNearClipped[f] = true;
                int32_t depthAverage = ((vertexScreenZPtr[a] + vertexScreenZPtr[b] + vertexScreenZPtr[c]) / 3) + minDepth;
                tmpDepthFaces[depthAverage][tmpDepthFaceCount[depthAverage]++] = f;
            } else {
                if (picking && PointWithinTriangle(mouseX, mouseY, vertexScreenYPtr[a], vertexScreenYPtr[b], vertexScreenYPtr[c], xA, xB, xC)) {
                    pickedBitsets[pickedCount++] = bitset;
                    picking = false;
                }

                // Back-face culling
                const int32_t yA = vertexScreenYPtr[a];
                const int32_t yB = vertexScreenYPtr[b];
                const int32_t yC = vertexScreenYPtr[c];
                const int32_t dxAB = xA - xB;
                const int32_t dyAB = yA - yB;
                const int32_t dxCB = xC - xB;
                const int32_t dyCB = yC - yB;

                if (((dxAB * dyCB) - (dyAB * dxCB)) <= 0) {
                    continue;
                }

                faceNearClipped[f] = false;
                faceClippedX[f] = (xA < 0) || (xB < 0) || (xC < 0) || (xA > boundXCached) || (xB > boundXCached) || (xC > boundXCached);

                int32_t depthAverage = ((vertexScreenZPtr[a] + vertexScreenZPtr[b] + vertexScreenZPtr[c]) / 3) + minDepth;
                int32_t index = tmpDepthFaceCount[depthAverage];
                tmpDepthFaces[depthAverage][index] = f;
                tmpDepthFaceCount[depthAverage]++;
            }
        }

        if (facePriority.empty()) {
            for (int32_t depth = maxDepth - 1; depth >= 0; depth--) {
                int32_t count = tmpDepthFaceCount[depth];
                if (count > 0) {
                    auto faces = tmpDepthFaces[depth];
                    for (int32_t f = 0; f < count; f++) {
                        DrawFace(faces[f]);
                    }
                }
            }
            return;
        }

        for (int32_t priority = 0; priority < 12; priority++) {
            Model::tmpPriorityFaceCount[priority] = 0;
            tmpPriorityDepthSum[priority] = 0;
        }

        for (int32_t depth = maxDepth - 1; depth >= 0; depth--) {
            int32_t faceCount = tmpDepthFaceCount[depth];

            if (faceCount <= 0) {
                continue;
            }

            auto faces = tmpDepthFaces[depth];

            for (int32_t i = 0; i < faceCount; i++) {
                int32_t face = faces[i];
                int32_t priority = facePriority[face];
                int32_t count = Model::tmpPriorityFaceCount[priority]++;
                Model::tmpPriorityFaces[priority][count] = face;

                if (priority < 10) {
                    tmpPriorityDepthSum[priority] += depth;
                } else if (priority == 10) {
                    tmpPriority10FaceDepth[count] = depth;
                } else {
                    tmpPriority11FaceDepth[count] = depth;
                }
            }
        }

        // These are as the name implies, the average depth between two priorities.
        int32_t averagePriorityDepth1_2 = 0;
        int32_t averagePriorityDepth3_4 = 0;
        int32_t averagePriorityDepth6_8 = 0;

        // Don't think too hard about it. It's just a way of calculating averages with integers but with two sums averaged together.

        if ((Model::tmpPriorityFaceCount[1] > 0) || (Model::tmpPriorityFaceCount[2] > 0)) {
            averagePriorityDepth1_2 = (tmpPriorityDepthSum[1] + tmpPriorityDepthSum[2]) / (Model::tmpPriorityFaceCount[1] + Model::tmpPriorityFaceCount[2]);
        }

        if ((Model::tmpPriorityFaceCount[3] > 0) || (Model::tmpPriorityFaceCount[4] > 0)) {
            averagePriorityDepth3_4 = (tmpPriorityDepthSum[3] + tmpPriorityDepthSum[4]) / (Model::tmpPriorityFaceCount[3] + Model::tmpPriorityFaceCount[4]);
        }

        if ((Model::tmpPriorityFaceCount[6] > 0) || (Model::tmpPriorityFaceCount[8] > 0)) {
            averagePriorityDepth6_8 = (tmpPriorityDepthSum[6] + tmpPriorityDepthSum[8]) / (Model::tmpPriorityFaceCount[6] + Model::tmpPriorityFaceCount[8]);
        }

        int32_t priorityFace = 0;
        int32_t priorityFaceCount = Model::tmpPriorityFaceCount[10];
        auto& priorityFaces = Model::tmpPriorityFaces[10];
        auto& priorityFaceDepths = tmpPriority10FaceDepth;

        if (priorityFace == priorityFaceCount) {
            priorityFace = 0;
            priorityFaceCount = Model::tmpPriorityFaceCount[11];
            priorityFaces = Model::tmpPriorityFaces[11];
            priorityFaceDepths = tmpPriority11FaceDepth;
        }

        int32_t priorityDepth;
        if (priorityFace < priorityFaceCount) {
            priorityDepth = priorityFaceDepths[priorityFace];
        } else {
            priorityDepth = -1000;
        }

        // The code below essentially gives priorities 10 and 11 a chance to draw during priorities 0, 3, and 5 as long
        // as the current face depth is slightly deeper than the current priority depth. You can think of that slightly
        // deeper concept as a threshold to allow lower priority triangles to draw on top of higher priority triangles.

        // If they didn't do this then higher priority triangles would always draw on top, which look weird.

        for (int32_t priority = 0; priority < 10; priority++) {
            while ((priority == 0) && (priorityDepth > averagePriorityDepth1_2)) {
                DrawFace(priorityFaces[priorityFace++]);

                if ((priorityFace == priorityFaceCount) && (priorityFaces != Model::tmpPriorityFaces[11])) {
                    priorityFace = 0;
                    priorityFaceCount = Model::tmpPriorityFaceCount[11];
                    priorityFaces = Model::tmpPriorityFaces[11];
                    priorityFaceDepths = tmpPriority11FaceDepth;
                }

                if (priorityFace < priorityFaceCount) {
                    priorityDepth = priorityFaceDepths[priorityFace];
                } else {
                    priorityDepth = -1000;
                }
            }

            while ((priority == 3) && (priorityDepth > averagePriorityDepth3_4)) {
                DrawFace(priorityFaces[priorityFace++]);

                if ((priorityFace == priorityFaceCount) && (priorityFaces != Model::tmpPriorityFaces[11])) {
                    priorityFace = 0;
                    priorityFaceCount = Model::tmpPriorityFaceCount[11];
                    priorityFaces = Model::tmpPriorityFaces[11];
                    priorityFaceDepths = tmpPriority11FaceDepth;
                }

                if (priorityFace < priorityFaceCount) {
                    priorityDepth = priorityFaceDepths[priorityFace];
                } else {
                    priorityDepth = -1000;
                }
            }

            while ((priority == 5) && (priorityDepth > averagePriorityDepth6_8)) {
                DrawFace(priorityFaces[priorityFace++]);

                if ((priorityFace == priorityFaceCount) && (priorityFaces != Model::tmpPriorityFaces[11])) {
                    priorityFace = 0;
                    priorityFaceCount = Model::tmpPriorityFaceCount[11];
                    priorityFaces = Model::tmpPriorityFaces[11];
                    priorityFaceDepths = tmpPriority11FaceDepth;
                }

                if (priorityFace < priorityFaceCount) {
                    priorityDepth = priorityFaceDepths[priorityFace];
                } else {
                    priorityDepth = -1000;
                }
            }

            int32_t count = Model::tmpPriorityFaceCount[priority];
            auto& faces = Model::tmpPriorityFaces[priority];

            for (int32_t i = 0; i < count; i++) {
                DrawFace(faces[i]);
            }
        }

        // finish off remaining faces (priorities 10 and 11)

        while (priorityDepth != -1000) {
            DrawFace(priorityFaces[priorityFace++]);

            if ((priorityFace == priorityFaceCount) && (priorityFaces != Model::tmpPriorityFaces[11])) {
                priorityFace = 0;
                priorityFaces = Model::tmpPriorityFaces[11];
                priorityFaceCount = Model::tmpPriorityFaceCount[11];
                priorityFaceDepths = tmpPriority11FaceDepth;
            }

            if (priorityFace < priorityFaceCount) {
                priorityDepth = priorityFaceDepths[priorityFace];
            } else {
                priorityDepth = -1000;
            }
        }

    }

    /**
     * Draws this model.
     *
     * @param yaw         the yaw of this model.
     * @param sinEyePitch the sin(eyePitch).
     * @param cosEyePitch the cos(eyePitch).
     * @param sinEyeYaw   the sin(eyeYaw).
     * @param cosEyeYaw   the cos(eyeYaw).
     * @param relativeX   the relative x. (SceneX - EyeX)
     * @param relativeY   the relative y. (SceneY - EyeY)
     * @param relativeZ   the relative z. (SceneZ - EyeZ)
     * @param bitset      the bitset.
     */
    void Model::Draw(int32_t yaw, int32_t sinEyePitch, int32_t cosEyePitch, int32_t sinEyeYaw, int32_t cosEyeYaw,
        int32_t relativeX, int32_t relativeY, int32_t relativeZ, int32_t bitset) {

        // Relative coordinates are ScenePos - EyePos

        // z' is our relative z value rotated by eye yaw.
        int32_t zPrime = ((relativeZ * cosEyeYaw) - (relativeX * sinEyeYaw)) >> 16;

        // midZ is our relative z value rotated by eye yaw and pitch. It's the distance from the camera from the center
        // of our model.
        int32_t midZ = ((relativeY * sinEyePitch) + (zPrime * cosEyePitch)) >> 16;

        // Our pitch is clamped between 128 and 384 (22.5 degrees and 67.5 degrees)
        // We know this will be positive and within 92->38% its original value.
        int32_t radiusCosEyePitch = (radius * cosEyePitch) >> 16;

        // +Z goes forward, which makes this value supposedly the farthest Z the model should be away from the camera.
        int32_t mZ = midZ + radiusCosEyePitch;

        // early z testing
        if ((mZ <= 50) || (midZ >= 3500)) {
            return;
        }

        // calculate x'
        int32_t midX = ((relativeZ * sinEyeYaw) + (relativeX * cosEyeYaw)) >> 16;

        // calculate left bound
        int32_t leftX = (midX - radius) << 9;

        // early fail
        if ((leftX / mZ) >= Draw2D::centerX) {
            return;
        }

        // calculate right bound
        int32_t rightX = (midX + radius) << 9;

        // early fail
        if ((rightX / mZ) <= -Draw2D::centerX) {
            return;
        }

        // midY is our relative y value rotated by eye pitch
        int32_t midY = ((relativeY * cosEyePitch) - (zPrime * sinEyePitch)) >> 16;

        // Our pitch is clamped between 128 and 384 (22.5 degrees and 67.5 degrees)
        // We know this will be positive and within 38->92% its original value.
        int32_t radiusSinEyePitch = (radius * sinEyePitch) >> 16;

        // calculate bottom bound
        int32_t bottomY = (midY + radiusSinEyePitch) << 9;

        // early fail
        if ((bottomY / mZ) <= -Draw2D::centerY) {
            return;
        }

        // y' = (radius * sin(eyePitch)) + (minY * cos(eyePitch))
        int32_t yPrime = radiusSinEyePitch + ((minY * cosEyePitch) >> 16);

        // calculate top boundary
        int32_t topY = (midY - yPrime) << 9;

        // early fail
        if ((topY / mZ) >= Draw2D::centerY) {
            return;
        }

        // (minY * sin(eyePitch)) + (radius * cos(eyePitch))
        int32_t radiusZ = ((minY * sinEyePitch) >> 16) + radiusCosEyePitch;

        bool clipped = (midZ - radiusZ) <= 50;
        bool picking = false;

        if ((bitset > 0) && Model::checkHover) {
            int32_t z = midZ - radiusCosEyePitch;

            if (z <= 50) {
                z = 50;
            }

            if (midX > 0) {
                leftX /= mZ;
                rightX /= z;
            } else {
                rightX /= mZ;
                leftX /= z;
            }

            if (midY > 0) {
                topY /= mZ;
                bottomY /= z;
            } else {
                bottomY /= mZ;
                topY /= z;
            }

            int32_t mX = Model::mouseX - Draw3D::centerX;
            int32_t mY = Model::mouseY - Draw3D::centerY;

            if ((mX > leftX) && (mX < rightX) && (mY > topY) && (mY < bottomY)) {
                if (pickable) {
                    pickedBitsets[pickedCount++] = bitset;
                } else {
                    picking = true;
                }
            }
        }

        int32_t centerX = Draw3D::centerX;
        int32_t centerY = Draw3D::centerY;
        int32_t sinYaw = 0;
        int32_t cosYaw = 0;

        if (yaw != 0) {
            sinYaw = Draw3D::sin[yaw];
            cosYaw = Draw3D::cos[yaw];
        }

        for (int32_t v = 0; v < vertexCount; v++) {
            int32_t x = vertexX[v];
            int32_t y = vertexY[v];
            int32_t z = vertexZ[v];

            // Local Space -> Model Space

            if (yaw != 0) {
                int32_t x_ = ((z * sinYaw) + (x * cosYaw)) >> 16;
                z = ((z * cosYaw) - (x * sinYaw)) >> 16;
                x = x_;
            }

            // Model Space -> View Space

            x += relativeX;
            y += relativeY;
            z += relativeZ;

            // Rotate on Y axis (Yaw)
            int32_t tmp = ((z * sinEyeYaw) + (x * cosEyeYaw)) >> 16;
            z = ((z * cosEyeYaw) - (x * sinEyeYaw)) >> 16;
            x = tmp;

            // Rotate on X axis (Pitch)
            tmp = ((y * cosEyePitch) - (z * sinEyePitch)) >> 16;
            z = ((y * sinEyePitch) + (z * cosEyePitch)) >> 16;
            y = tmp;

            if (z >= 50) {
                vertexScreenX[v] = centerX + ((x << 9) / z);
                vertexScreenY[v] = centerY + ((y << 9) / z);
            } else {
                vertexScreenX[v] = -5000; // used in drawTriangle to denote a near-clipped triangle.
                clipped = true;
            }

            vertexScreenZ[v] = z - midZ;

            if (clipped || (texturedFaceCount > 0)) {
                vertexViewSpaceX[v] = x;
                vertexViewSpaceY[v] = y;
                vertexViewSpaceZ[v] = z;
            }
        }
        Draw(clipped, picking, bitset);
    }

    /**
     * Draws the provided face id assuming it has been clipped by the near Z plane.
     *
     * @param face the face id.
     */
    void Model::DrawNearClippedFace(int32_t face) {
        const int32_t centerX = Draw3D::centerX;
        const int32_t centerY = Draw3D::centerY;
        int32_t elements = 0;

        const int32_t a = faceVertexA[face];
        const int32_t b = faceVertexB[face];
        const int32_t c = faceVertexC[face];

        const int32_t zA = vertexViewSpaceZ[a];
        const int32_t zB = vertexViewSpaceZ[b];
        const int32_t zC = vertexViewSpaceZ[c];

        if (zA >= 50) {
            clippedX[elements] = vertexScreenX[a];
            clippedY[elements] = vertexScreenY[a];
            clippedColor[elements++] = faceColorA[face];
        } else {
            int32_t xA = vertexViewSpaceX[a];
            int32_t yA = vertexViewSpaceY[a];
            int32_t colorA = faceColorA[face];

            if (zC >= 50) {
                int32_t scalar = (50 - zA) * Draw3D::reciprocal16[zC - zA];
                clippedX[elements] = centerX + (((xA + (((vertexViewSpaceX[c] - xA) * scalar) >> 16)) << 9) / 50);
                clippedY[elements] = centerY + (((yA + (((vertexViewSpaceY[c] - yA) * scalar) >> 16)) << 9) / 50);
                clippedColor[elements++] = colorA + (((faceColorC[face] - colorA) * scalar) >> 16);
            }

            if (zB >= 50) {
                int32_t scalar = (50 - zA) * Draw3D::reciprocal16[zB - zA];
                clippedX[elements] = centerX + (((xA + (((vertexViewSpaceX[b] - xA) * scalar) >> 16)) << 9) / 50);
                clippedY[elements] = centerY + (((yA + (((vertexViewSpaceY[b] - yA) * scalar) >> 16)) << 9) / 50);
                clippedColor[elements++] = colorA + (((faceColorB[face] - colorA) * scalar) >> 16);
            }
        }

        if (zB >= 50) {
            clippedX[elements] = vertexScreenX[b];
            clippedY[elements] = vertexScreenY[b];
            clippedColor[elements++] = faceColorB[face];
        } else {
            int32_t xB = vertexViewSpaceX[b];
            int32_t yB = vertexViewSpaceY[b];
            int32_t colorB = faceColorB[face];

            if (zA >= 50) {
                int32_t scalar = (50 - zB) * Draw3D::reciprocal16[zA - zB];
                clippedX[elements] = centerX + (((xB + (((vertexViewSpaceX[a] - xB) * scalar) >> 16)) << 9) / 50);
                clippedY[elements] = centerY + (((yB + (((vertexViewSpaceY[a] - yB) * scalar) >> 16)) << 9) / 50);
                clippedColor[elements++] = colorB + (((faceColorA[face] - colorB) * scalar) >> 16);
            }

            if (zC >= 50) {
                int32_t scalar = (50 - zB) * Draw3D::reciprocal16[zC - zB];
                clippedX[elements] = centerX + (((xB + (((vertexViewSpaceX[c] - xB) * scalar) >> 16)) << 9) / 50);
                clippedY[elements] = centerY + (((yB + (((vertexViewSpaceY[c] - yB) * scalar) >> 16)) << 9) / 50);
                clippedColor[elements++] = colorB + (((faceColorC[face] - colorB) * scalar) >> 16);
            }
        }

        if (zC >= 50) {
            clippedX[elements] = vertexScreenX[c];
            clippedY[elements] = vertexScreenY[c];
            clippedColor[elements++] = faceColorC[face];
        } else {
            int32_t xC = vertexViewSpaceX[c];
            int32_t yC = vertexViewSpaceY[c];
            int32_t colorC = faceColorC[face];

            if (zB >= 50) {
                int32_t k6 = (50 - zC) * Draw3D::reciprocal16[zB - zC];
                clippedX[elements] = centerX + (((xC + (((vertexViewSpaceX[b] - xC) * k6) >> 16)) << 9) / 50);
                clippedY[elements] = centerY + (((yC + (((vertexViewSpaceY[b] - yC) * k6) >> 16)) << 9) / 50);
                clippedColor[elements++] = colorC + (((faceColorB[face] - colorC) * k6) >> 16);
            }

            if (zA >= 50) {
                int32_t l6 = (50 - zC) * Draw3D::reciprocal16[zA - zC];
                clippedX[elements] = centerX + (((xC + (((vertexViewSpaceX[a] - xC) * l6) >> 16)) << 9) / 50);
                clippedY[elements] = centerY + (((yC + (((vertexViewSpaceY[a] - yC) * l6) >> 16)) << 9) / 50);
                clippedColor[elements++] = colorC + (((faceColorA[face] - colorC) * l6) >> 16);
            }
        }

        int32_t x0 = clippedX[0];
        int32_t x1 = clippedX[1];
        int32_t x2 = clippedX[2];
        int32_t y0 = clippedY[0];
        int32_t y1 = clippedY[1];
        int32_t y2 = clippedY[2];

        // Back-face culling
        if ((((x0 - x1) * (y2 - y1)) - ((y0 - y1) * (x2 - x1))) <= 0) {
            return;
        }

        Draw3D::clipX = false;

        // It's possible for a single triangle to be clipped into two separate triangles.

        if (elements == 3) {
            if ((x0 < 0) || (x1 < 0) || (x2 < 0) || (x0 > Draw2D::boundX) || (x1 > Draw2D::boundX) || (x2 > Draw2D::boundX)) {
                Draw3D::clipX = true;
            }

            int32_t type;

            if (faceInfo.empty()) {
                type = 0;
            } else {
                type = faceInfo[face] & 3;
            }

            if (type == 0) {
                Draw3D::FillGouraudTriangle(y0, y1, y2, x0, x1, x2, clippedColor[0], clippedColor[1], clippedColor[2]);
            } else if (type == 1) {
                Draw3D::FillTriangle(y0, y1, y2, x0, x1, x2, Draw3D::palette[faceColorA[face]]);
            } else if (type == 2) {
                int32_t texturedFace = faceInfo[face] >> 2;
                int32_t tA = texturedVertexA[texturedFace];
                int32_t tB = texturedVertexB[texturedFace];
                int32_t tC = texturedVertexC[texturedFace];
                Draw3D::FillTexturedTriangle(y0, y1, y2, x0, x1, x2, clippedColor[0], clippedColor[1], clippedColor[2], vertexViewSpaceX[tA], vertexViewSpaceX[tB], vertexViewSpaceX[tC], vertexViewSpaceY[tA], vertexViewSpaceY[tB], vertexViewSpaceY[tC], vertexViewSpaceZ[tA], vertexViewSpaceZ[tB], vertexViewSpaceZ[tC], faceColor[face]);
            } else if (type == 3) {
                int texturedFace = faceInfo[face] >> 2;
                int32_t tA = texturedVertexA[texturedFace];
                int32_t tB = texturedVertexB[texturedFace];
                int32_t tC = texturedVertexC[texturedFace];
                Draw3D::FillTexturedTriangle(y0, y1, y2, x0, x1, x2, faceColorA[face], faceColorA[face], faceColorA[face], vertexViewSpaceX[tA], vertexViewSpaceX[tB], vertexViewSpaceX[tC], vertexViewSpaceY[tA], vertexViewSpaceY[tB], vertexViewSpaceY[tC], vertexViewSpaceZ[tA], vertexViewSpaceZ[tB], vertexViewSpaceZ[tC], faceColor[face]);
            }
        } else if (elements == 4) {
            if ((x0 < 0) || (x1 < 0) || (x2 < 0) || (x0 > Draw2D::boundX) || (x1 > Draw2D::boundX) || (x2 > Draw2D::boundX) || (clippedX[3] < 0) || (clippedX[3] > Draw2D::boundX)) {
                Draw3D::clipX = true;
            }

            int32_t type;

            if (faceInfo.empty()) {
                type = 0;
            } else {
                type = faceInfo[face] & 3;
            }

            if (type == 0) {
                Draw3D::FillGouraudTriangle(y0, y1, y2, x0, x1, x2, clippedColor[0], clippedColor[1], clippedColor[2]);
                Draw3D::FillGouraudTriangle(y0, y2, clippedY[3], x0, x2, clippedX[3], clippedColor[0], clippedColor[2], clippedColor[3]);
            } else if (type == 1) {
                int32_t colorA = Draw3D::palette[faceColorA[face]];
                Draw3D::FillTriangle(y0, y1, y2, x0, x1, x2, colorA);
                Draw3D::FillTriangle(y0, y2, clippedY[3], x0, x2, clippedX[3], colorA);
            } else if (type == 2) {
                int32_t texturedFace = faceInfo[face] >> 2;
                int32_t tA = texturedVertexA[texturedFace];
                int32_t tB = texturedVertexB[texturedFace];
                int32_t tC = texturedVertexC[texturedFace];
                Draw3D::FillTexturedTriangle(y0, y1, y2, x0, x1, x2, clippedColor[0], clippedColor[1], clippedColor[2], vertexViewSpaceX[tA], vertexViewSpaceX[tB], vertexViewSpaceX[tC], vertexViewSpaceY[tA], vertexViewSpaceY[tB], vertexViewSpaceY[tC], vertexViewSpaceZ[tA], vertexViewSpaceZ[tB], vertexViewSpaceZ[tC], faceColor[face]);
                Draw3D::FillTexturedTriangle(y0, y2, clippedY[3], x0, x2, clippedX[3], clippedColor[0], clippedColor[2], clippedColor[3], vertexViewSpaceX[tA], vertexViewSpaceX[tB], vertexViewSpaceX[tC], vertexViewSpaceY[tA], vertexViewSpaceY[tB], vertexViewSpaceY[tC], vertexViewSpaceZ[tA], vertexViewSpaceZ[tB], vertexViewSpaceZ[tC], faceColor[face]);
            } else if (type == 3) {
                int32_t texturedFace = faceInfo[face] >> 2;
                int32_t tA = texturedVertexA[texturedFace];
                int32_t tB = texturedVertexB[texturedFace];
                int32_t tC = texturedVertexC[texturedFace];
                Draw3D::FillTexturedTriangle(y0, y1, y2, x0, x1, x2, faceColorA[face], faceColorA[face], faceColorA[face], vertexViewSpaceX[tA], vertexViewSpaceX[tB], vertexViewSpaceX[tC], vertexViewSpaceY[tA], vertexViewSpaceY[tB], vertexViewSpaceY[tC], vertexViewSpaceZ[tA], vertexViewSpaceZ[tB], vertexViewSpaceZ[tC], faceColor[face]);
                Draw3D::FillTexturedTriangle(y0, y2, clippedY[3], x0, x2, clippedX[3], faceColorA[face], faceColorA[face], faceColorA[face], vertexViewSpaceX[tA], vertexViewSpaceX[tB], vertexViewSpaceX[tC], vertexViewSpaceY[tA], vertexViewSpaceY[tB], vertexViewSpaceY[tC], vertexViewSpaceZ[tA], vertexViewSpaceZ[tB], vertexViewSpaceZ[tC], faceColor[face]);
            }
        }
    }

    /**
     * Recalculates the normals and optionally applies the lighting immediately after. This method <i>can</i> be
     * destructive if <code>applyLighting</code> is set to <code>true</code>.
     *
     * @param lightAmbient     the ambient light.
     * @param lightAttenuation the light attenuation.
     * @param lightSrcX        the light source x.
     * @param lightSrcY        the light source y.
     * @param lightSrcZ        the light source z.
     * @param applyLighting    <code>true</code> to invoke {@link #applyLighting(int, int, int, int, int)} after normals are
     *                         calculated.
     * @see #applyLighting(int, int, int, int, int)
     */
    void Model::CalculateNormals(const int32_t lightAmbient, const int32_t lightAttenuation, const int32_t lightSrcX, const int32_t lightSrcY,
        const int32_t lightSrcZ, const bool applyLighting) {
        const auto lightMagnitude = static_cast<int32_t>(std::sqrt((lightSrcX * lightSrcX) + (lightSrcY * lightSrcY) + (lightSrcZ * lightSrcZ)));
        const int32_t attenuation = (lightAttenuation * lightMagnitude) >> 8;

        if (faceColorA.empty()) {
            faceColorA = std::vector<int32_t>(faceCount);
            faceColorB = std::vector<int32_t>(faceCount);
            faceColorC = std::vector<int32_t>(faceCount);
        }

        if (vertexNormal.empty()) {
            vertexNormal.resize(vertexCount);
            for (int32_t v = 0; v < vertexCount; v++) {
                vertexNormal[v] = VertexNormal{0, 0, 0, 0};
            }
        }

        for (int32_t f = 0; f < faceCount; f++) {
            const int32_t a = faceVertexA[f];
            const int32_t b = faceVertexB[f];
            const int32_t c = faceVertexC[f];

            const int32_t dxAB = vertexX[b] - vertexX[a];
            const int32_t dyAB = vertexY[b] - vertexY[a];
            const int32_t dzAB = vertexZ[b] - vertexZ[a];

            const int32_t dxAC = vertexX[c] - vertexX[a];
            const int32_t dyAC = vertexY[c] - vertexY[a];
            const int32_t dzAC = vertexZ[c] - vertexZ[a];

            int32_t nx = (dyAB * dzAC) - (dyAC * dzAB);
            int32_t ny = (dzAB * dxAC) - (dzAC * dxAB);
            int32_t nz = (dxAB * dyAC) - (dxAC * dyAB);

            while ((nx > 8192) || (ny > 8192) || (nz > 8192) || (nx < -8192) || (ny < -8192) || (nz < -8192)) {
                nx >>= 1;
                ny >>= 1;
                nz >>= 1;
            }

            auto length = static_cast<int32_t>(std::sqrt((nx * nx) + (ny * ny) + (nz * nz)));

            if (length <= 0) {
                length = 1;
            }

            // normalize
            nx = (nx * 256) / length;
            ny = (ny * 256) / length;
            nz = (nz * 256) / length;

            if ((faceInfo.empty()) || ((faceInfo[f] & 1) == 0)) {
                VertexNormal& nA = vertexNormal[a];
                nA.x += nx; nA.y += ny; nA.z += nz; nA.w++;

                VertexNormal& nB = vertexNormal[b];
                nB.x += nx; nB.y += ny; nB.z += nz; nB.w++;

                VertexNormal& nC = vertexNormal[c];
                nC.x += nx; nC.y += ny; nC.z += nz; nC.w++;
            } else {
                const int32_t lightness = lightAmbient + (((lightSrcX * nx) + (lightSrcY * ny) + (lightSrcZ * nz)) / (attenuation + (attenuation / 2)));
                faceColorA[f] = MulColorLightness(faceColor[f], lightness, faceInfo[f]);
            }
        }

        if (applyLighting) {
            ApplyLighting(lightAmbient, attenuation, lightSrcX, lightSrcY, lightSrcZ);
        } else {
            vertexNormalOriginal.resize(vertexCount);

            for (int32_t v = 0; v < vertexCount; ++v) {
                const VertexNormal& src = vertexNormal[v];
                vertexNormalOriginal[v] = VertexNormal{src.x, src.y, src.z, src.w};
            }
        }

        if (applyLighting) {
            CalculateBoundsCylinder();
        } else {
            CalculateBoundsAABB();
        }

    }

    /**
     * Calculates the lightness values for all faces using the provided lighting parameters. This method is destructive
     * and nullifies normals and labels, which means it is meant to be applied once to a model.
     *
     * @param lightAmbient     the ambient lighting value. [0...127]
     * @param lightAttenuation the light attenuation.
     * @param lightSrcX        the light source x.
     * @param lightSrcY        the light source y.
     * @param lightSrcZ        the light source z.
     */
    void Model::ApplyLighting(const int32_t lightAmbient, const int32_t lightAttenuation, const int32_t lightSrcX, const int32_t lightSrcY,
        const int32_t lightSrcZ) {
        for (int32_t f = 0; f < faceCount; f++) {
            const int32_t a = faceVertexA[f];
            const int32_t b = faceVertexB[f];
            const int32_t c = faceVertexC[f];

            if (faceInfo.empty()) {
                int32_t color = faceColor[f];

                VertexNormal& nA = vertexNormal[a];
                int32_t lightnessA = lightAmbient + ((lightSrcX * nA.x + lightSrcY * nA.y + lightSrcZ * nA.z) / (lightAttenuation * nA.w));
                faceColorA[f] = MulColorLightness(color, lightnessA, 0);

                VertexNormal& nB = vertexNormal[b];
                int32_t lightnessB = lightAmbient + ((lightSrcX * nB.x + lightSrcY * nB.y + lightSrcZ * nB.z) / (lightAttenuation * nB.w));
                faceColorB[f] = MulColorLightness(color, lightnessB, 0);

                VertexNormal& nC = vertexNormal[c];
                int32_t lightnessC = lightAmbient + ((lightSrcX * nC.x + lightSrcY * nC.y + lightSrcZ * nC.z) / (lightAttenuation * nC.w));
                faceColorC[f] = MulColorLightness(color, lightnessC, 0);
            } else if ((faceInfo[f] & 1) == 0) {
                int32_t color = faceColor[f];
                int32_t info = faceInfo[f];

                VertexNormal& nA = vertexNormal[a];
                int32_t lightnessA = lightAmbient + ((lightSrcX * nA.x + lightSrcY * nA.y + lightSrcZ * nA.z) / (lightAttenuation * nA.w));
                faceColorA[f] = MulColorLightness(color, lightnessA, info);

                VertexNormal& nB = vertexNormal[b];
                int32_t lightnessB = lightAmbient + ((lightSrcX * nB.x + lightSrcY * nB.y + lightSrcZ * nB.z) / (lightAttenuation * nB.w));
                faceColorB[f] = MulColorLightness(color, lightnessB, info);

                VertexNormal& nC = vertexNormal[c];
                int32_t lightnessC = lightAmbient + ((lightSrcX * nC.x + lightSrcY * nC.y + lightSrcZ * nC.z) / (lightAttenuation * nC.w));
                faceColorC[f] = MulColorLightness(color, lightnessC, info);
            }
        }

        vertexNormal.clear();
        vertexNormalOriginal.clear();
        vertexLabel.clear();
        faceLabel.clear();

        if (!faceInfo.empty()) {
            for (int32_t f = 0; f < faceCount; f++) {
                if ((faceInfo[f] & 2) == 2) {
                    return;
                }
            }
        }

        faceColor.clear();
    }

    /**
     * Sets <code>this</code> model to the provided model using a vertex pool.
     * <p>
     * Face alpha may be optionally copied in case an applied {@link SeqTransform} modifies the alpha of the model.
     *
     * @param model      the model.
     * @param shareAlpha whether to copy or share a reference to face alphas.
     */
    void Model::Set(Model& model, bool shareAlpha)
    {
        vertexCount = model.vertexCount;
        faceCount = model.faceCount;
        texturedFaceCount = model.texturedFaceCount;

        if (tmpVertexX.size() < vertexCount) {
            tmpVertexX.resize(vertexCount + 100);
            tmpVertexY.resize(vertexCount + 100);
            tmpVertexZ.resize(vertexCount + 100);
        }

        vertexX = tmpVertexX;
        vertexY = tmpVertexY;
        vertexZ = tmpVertexZ;

        for (int32_t k = 0; k < vertexCount; k++) {
            vertexX[k] = model.vertexX[k];
            vertexY[k] = model.vertexY[k];
            vertexZ[k] = model.vertexZ[k];
        }

        if (shareAlpha) {
            faceAlpha = model.faceAlpha;
        } else {
            if (tmpFaceAlpha.size() < faceCount) {
                tmpFaceAlpha.resize(vertexCount + 100);
            }
            faceAlpha = tmpFaceAlpha;
            if (model.faceAlpha.empty()) {
                for (int32_t face = 0; face < faceCount; face++) {
                    faceAlpha[face] = 0;
                }
            } else {
                for (int32_t face = 0; face < faceCount; face++) {
                    faceAlpha[face] = model.faceAlpha[face];
                }
            }
        }

        faceInfo = model.faceInfo;
        faceColor = model.faceColor;
        facePriority = model.facePriority;
        priority = model.priority;
        labelFaces = model.labelFaces;
        labelVertices = model.labelVertices;
        faceVertexA = model.faceVertexA;
        faceVertexB = model.faceVertexB;
        faceVertexC = model.faceVertexC;
        faceColorA = model.faceColorA;
        faceColorB = model.faceColorB;
        faceColorC = model.faceColorC;
        texturedVertexA = model.texturedVertexA;
        texturedVertexB = model.texturedVertexB;
        texturedVertexC = model.texturedVertexC;
    }

    /**
     * Applies the specified {@link SeqTransform} ids.
     *
     * @param primaryID   the primary transform id.
     * @param secondaryID the secondary transform id.
     * @param mask        the mask contains base ids to prevent the primary transform from using the same bases as the secondary transform.
     */
    void Model::ApplyTransforms(int32_t primaryID, int32_t secondaryID, std::vector<int32_t>& mask)
    {
        if (primaryID == -1) {
            return;
        }

        if ((mask.empty()) || (secondaryID == -1)) {
            ApplyTransform(primaryID);
            return;
        }

        const auto& primary = SeqTransform::Get(primaryID);

        if (primary == nullptr) {
            return;
        }

        const auto& secondary = SeqTransform::Get(secondaryID);

        if (secondary == nullptr) {
            ApplyTransform(primaryID);
            return;
        }

        const auto& skeleton = primary->skeleton;

        baseX = 0;
        baseY = 0;
        baseZ = 0;

        int32_t counter = 0;
        int32_t maskBase = mask[counter++];

        for (int32_t i = 0; i < primary->length; i++) {
            int32_t base = primary->bases[i];

            while (base > maskBase) {
                maskBase = mask[counter++];
            }

            if ((base != maskBase) || (skeleton.baseTypes[base] == 0)) {
                ApplyTransform(skeleton.baseTypes[base], skeleton.baseLabels[base], primary->x[i], primary->y[i], primary->z[i]);
            }
        }

        baseX = 0;
        baseY = 0;
        baseZ = 0;

        counter = 0;
        maskBase = mask[counter++];

        for (int32_t i = 0; i < secondary->length; i++) {
            int32_t base = secondary->bases[i];

            while (base > maskBase) {
                maskBase = mask[counter++];
            }

            if ((base == maskBase) || (skeleton.baseTypes[base] == 0)) {
                ApplyTransform(skeleton.baseTypes[base], skeleton.baseLabels[base],
                    secondary->x[i], secondary->y[i], secondary->z[i]);
            }
        }
    }

    /**
     * Applies a transform of the given <code>type</code> to the specified list of <code>labels</code> using the parameters
     * <code>x, y, z</code>.
     *
     * @param type   the transform type.
     * @param labels the transform labels.
     * @param x      the param x.
     * @param y      the param y.
     * @param z      the param z.
     * @see #applyTransforms(int, int, int[])
     * @see #applyTransform(int)
     * @see SeqSkeleton#OP_BASE
     * @see SeqSkeleton#OP_TRANSLATE
     * @see SeqSkeleton#OP_ROTATE
     * @see SeqSkeleton#OP_SCALE
     * @see SeqSkeleton#OP_ALPHA
     */
    void Model::ApplyTransform(int32_t type, const std::vector<int32_t>& labels, int32_t x, int32_t y, int32_t z)
    {
        switch (type) {
            case SeqSkeleton::OP_BASE:
                {
                    int32_t count = 0;
                    baseX = 0;
                    baseY = 0;
                    baseZ = 0;
                    for (int32_t label : labels) {
                        if (label >= labelVertices.size()) {
                            continue;
                        }
                        auto& vertices = labelVertices[label];
                        for (int32_t v : vertices) {
                            baseX += vertexX[v];
                            baseY += vertexY[v];
                            baseZ += vertexZ[v];
                            count++;
                        }
                    }
                    if (count > 0) {
                        baseX = (baseX / count) + x;
                        baseY = (baseY / count) + y;
                        baseZ = (baseZ / count) + z;
                    } else {
                        baseX = x;
                        baseY = y;
                        baseZ = z;
                    }
                    break;
                }
            case SeqSkeleton::OP_TRANSLATE:
                {
                    for (int32_t group : labels) {
                        if (group >= labelVertices.size()) {
                            continue;
                        }
                        auto& vertices = labelVertices[group];
                        for (int32_t v : vertices) {
                            vertexX[v] += x;
                            vertexY[v] += y;
                            vertexZ[v] += z;
                        }
                    }
                    break;
                }
            case SeqSkeleton::OP_ROTATE:
                {
                    for (int32_t label : labels) {
                        if (label >= labelVertices.size()) {
                            continue;
                        }
                        auto& vertices = labelVertices[label];
                        for (int32_t v : vertices) {
                            vertexX[v] -= baseX;
                            vertexY[v] -= baseY;
                            vertexZ[v] -= baseZ;
                            int32_t pitch = (x & 0xff) * 8;
                            int32_t yaw = (y & 0xff) * 8;
                            int32_t roll = (z & 0xff) * 8;
                            if (roll != 0) {
                                int32_t sin = Draw3D::sin[roll];
                                int32_t cos = Draw3D::cos[roll];
                                int32_t x_ = ((vertexY[v] * sin) + (vertexX[v] * cos)) >> 16;
                                vertexY[v] = ((vertexY[v] * cos) - (vertexX[v] * sin)) >> 16;
                                vertexX[v] = x_;
                            }
                            if (pitch != 0) {
                                int32_t sin = Draw3D::sin[pitch];
                                int32_t cos = Draw3D::cos[pitch];
                                int32_t y_ = ((vertexY[v] * cos) - (vertexZ[v] * sin)) >> 16;
                                vertexZ[v] = ((vertexY[v] * sin) + (vertexZ[v] * cos)) >> 16;
                                vertexY[v] = y_;
                            }
                            if (yaw != 0) {
                                int32_t sin = Draw3D::sin[yaw];
                                int32_t cos = Draw3D::cos[yaw];
                                int32_t x_ = ((vertexZ[v] * sin) + (vertexX[v] * cos)) >> 16;
                                vertexZ[v] = ((vertexZ[v] * cos) - (vertexX[v] * sin)) >> 16;
                                vertexX[v] = x_;
                            }
                            vertexX[v] += baseX;
                            vertexY[v] += baseY;
                            vertexZ[v] += baseZ;
                        }
                    }
                    break;
                }
            case SeqSkeleton::OP_SCALE:
                {
                    for (int32_t label : labels) {
                        if (label >= labelVertices.size()) {
                            continue;
                        }
                        auto& vertices = labelVertices[label];
                        for (int32_t v : vertices) {
                            vertexX[v] -= baseX;
                            vertexY[v] -= baseY;
                            vertexZ[v] -= baseZ;
                            vertexX[v] = (vertexX[v] * x) / 128;
                            vertexY[v] = (vertexY[v] * y) / 128;
                            vertexZ[v] = (vertexZ[v] * z) / 128;
                            vertexX[v] += baseX;
                            vertexY[v] += baseY;
                            vertexZ[v] += baseZ;
                        }
                    }
                    break;
                }
            case SeqSkeleton::OP_ALPHA:
                {
                    if (!labelFaces.empty() && !faceAlpha.empty()) {
                        for (int32_t label : labels) {
                            if (label >= labelFaces.size()) {
                                continue;
                            }
                            auto& triangles = labelFaces[label];
                            for (int32_t t : triangles) {
                                faceAlpha[t] += x * 8;
                                if (faceAlpha[t] < 0) {
                                    faceAlpha[t] = 0;
                                }
                                if (faceAlpha[t] > 255) {
                                    faceAlpha[t] = 255;
                                }
                            }
                        }
                    }
                    break;
                }
        }
    }

    /**
     * Rotates the model on the X/Pitch axis.
     *
     * @param angle the angle.
     */
    void Model::RotateX(int32_t angle)
    {
        int32_t sin = Draw3D::sin[angle];
        int32_t cos = Draw3D::cos[angle];
        for (int32_t v = 0; v < vertexCount; v++) {
            int32_t tmp = ((vertexY[v] * cos) - (vertexZ[v] * sin)) >> 16;
            vertexZ[v] = ((vertexY[v] * sin) + (vertexZ[v] * cos)) >> 16;
            vertexY[v] = tmp;
        }
    }

    /**
     * Validates the provided model id. If the model is not loaded then a request is sent to the ondemand service.
     *
     * @param id the model id.
     * @return <code>true</code> if the model is loaded.
     */
    bool Model::Validate(int32_t id)
    {
        if (headers.empty()) {
            return false;
        }

        auto& header = headers[id];

        if (header.loaded) {
            return true;
        } else {
            ondemand->RequestModel(id);
            return false;
        }
    }

    void Model::Unload()
    {
        headers.clear();
        faceClippedX.fill(0);
        faceNearClipped.fill(0);
        vertexScreenX.fill(0);
        vertexScreenY.fill(0);
        vertexScreenZ.fill(0);
        vertexViewSpaceX.fill(0);
        vertexViewSpaceY.fill(0);
        vertexViewSpaceZ.fill(0);
        tmpDepthFaceCount.fill(0);
        //tmpDepthFaces.clear();
        tmpPriorityFaceCount.fill(0);
        tmpPriorityFaces.clear();
        tmpPriority10FaceDepth.fill(0);
        tmpPriority11FaceDepth.fill(0);
        tmpPriorityDepthSum.fill(0);
    }

    void Model::Unload(int32_t id)
    {
        headers[id] = Header();
    }

    /**
     * Calculates this models axis-aligned bounding box (AABB) and stores it.
     */
    void Model::CalculateBoundsAABB() {
        minY = 0;
        radius = 0;
        maxY = 0;
        minX = 999999;
        maxX = -999999;
        maxZ = -99999;
        minZ = 99999;
        for (int32_t j = 0; j < vertexCount; j++) {
            const int32_t x = vertexX[j];
            const int32_t y = vertexY[j];
            const int32_t z = vertexZ[j];
            if (x < minX) {
                minX = x;
            }
            if (x > maxX) {
                maxX = x;
            }
            if (z < minZ) {
                minZ = z;
            }
            if (z > maxZ) {
                maxZ = z;
            }
            if (-y > minY) {
                minY = -y;
            }
            if (y > maxY) {
                maxY = y;
            }
            int32_t radiusSqr = (x * x) + (z * z);
            if (radiusSqr > radius) {
                radius = radiusSqr;
            }
        }
        radius = static_cast<int32_t>(std::sqrt(radius));
        minDepth = static_cast<int32_t>(std::sqrt((radius * radius) + (minY * minY)));
        maxDepth = minDepth + static_cast<int32_t>(std::sqrt((radius * radius) + (maxY * maxY)));
    }

    /**
     * Adds <code>vertexId</code> from <code>src</code> to <code>this</code>. Reuses vertex if one already exists at the
     * same location.
     *
     * @param src      the source model.
     * @param vertexId the vertex id to add from the <code>src</code>.
     * @return the vertex id of the added vertex.
     */
    int32_t Model::AddVertex(Model& src, int32_t vertexId)
    {
        int32_t x = src.vertexX[vertexId];
        int32_t y = src.vertexY[vertexId];
        int32_t z = src.vertexZ[vertexId];

        int32_t identical = -1;
        for (int32_t v = 0; v < vertexCount; v++) {
            if ((x == vertexX[v]) && (y == vertexY[v]) && (z == vertexZ[v])) {
                identical = v;
                break;
            }
        }

        // append new one if no matches were found
        if (identical == -1) {
            vertexX[vertexCount] = x;
            vertexY[vertexCount] = y;
            vertexZ[vertexCount] = z;
            if (!src.vertexLabel.empty()) {
                vertexLabel[vertexCount] = src.vertexLabel[vertexId];
            }
            identical = vertexCount++;
        }

        return identical;
    }

    /**
     * Performs the equivalent operation of rotating the entire model on the Y axis by 180 degrees.
     */
    void Model::RotateY180() {
        for (int32_t v = 0; v < vertexCount; v++) {
            vertexZ[v] = -vertexZ[v];
        }
        for (int32_t f = 0; f < faceCount; f++) {
            int32_t a = faceVertexA[f];
            faceVertexA[f] = faceVertexC[f];
            faceVertexC[f] = a;
        }
    }

    /**
     * Rotates clockwise on the Y axis by 90 degrees.
     */
    void Model::RotateY90()
    {
        for (int32_t v = 0; v < vertexCount; v++) {
            int32_t tmp = vertexX[v];
            vertexX[v] = vertexZ[v];
            vertexZ[v] = -tmp;
        }
    }

    /**
     * Replaces use of the color <code>src</code> with color <code>dst</code>.
     *
     * @param src the source texture id.
     * @param dst the destination texture id.
     */
    void Model::Recolor(int32_t src, int32_t dst)
    {
        for (int32_t k = 0; k < faceCount; k++) {
            if (faceColor[k] == src) {
                faceColor[k] = dst;
            }
        }
    }

    /**
     * Scales the model. This method assumes 25.7 fixed-point integers, which means the value 1.0 will be <code>1<<7</code>
     * or 128.
     *
     * @param x the x scalar.
     * @param y the y scalar.
     * @param z the z scalar.
     */
    void Model::Scale(int32_t x, int32_t y, int32_t z)
    {
        for (int32_t v = 0; v < vertexCount; v++) {
            vertexX[v] = (vertexX[v] * x) / 128;
            vertexY[v] = (vertexY[v] * z) / 128;
            vertexZ[v] = (vertexZ[v] * y) / 128;
        }
    }

    /**
     * Translates the model by the offset provided.
     *
     * @param x the x.
     * @param y the y.
     * @param z the z.
     */
    void Model::Translate(int32_t x, int32_t y, int32_t z)
    {
        for (int32_t v = 0; v < vertexCount; v++) {
            vertexX[v] += x;
            vertexY[v] += y;
            vertexZ[v] += z;
        }
    }

    /**
     * Builds {@link #labelVertices} and {@link #labelFaces} using {@link #vertexLabel} and {@link #faceLabel}.
     * <p>
     * This method is <i>destructive</i>, meaning it sets {@link #vertexLabel} and {@link #faceLabel} to <code>null</code>
     * after being called.
     * <p>
     * This method is required for applying animations to a model and should only be called once on a single instance
     * of a {@link Model}.
     *
     * @see #applyTransform(int)
     * @see #applyTransforms(int, int, int[])
     * @see #applyTransform(int, int[], int, int, int)
     */
    void Model::CreateLabelReferences()
    {
        if (!vertexLabel.empty()) {
            std::vector<int32_t> labelVertexCount(256);

            int32_t count = 0;
            for (int32_t v = 0; v < vertexCount; v++) {
                int32_t label = vertexLabel[v];
                labelVertexCount[label]++;
                if (label > count) {
                    count = label;
                }
            }

            labelVertices.resize(count + 1);

            for (int32_t label = 0; label <= count; label++) {
                labelVertices[label].resize(labelVertexCount[label]);
                labelVertexCount[label] = 0;
            }

            for (int32_t v = 0; v < vertexCount; v++) {
                int32_t label = vertexLabel[v];
                labelVertices[label][labelVertexCount[label]++] = v;
            }

            vertexLabel.clear();
        }

        if (!faceLabel.empty()) {
            std::vector<int32_t> labelFaceCount(256);

            int32_t count = 0;
            for (int32_t f = 0; f < faceCount; f++) {
                int32_t label = faceLabel[f];
                labelFaceCount[label]++;
                if (label > count) {
                    count = label;
                }
            }

            labelFaces.resize(count + 1);
            for (int32_t label = 0; label <= count; label++) {
                labelFaces[label].resize(labelFaceCount[label]);
                labelFaceCount[label] = 0;
            }

            for (int32_t face = 0; face < faceCount; face++) {
                int32_t label = faceLabel[face];
                labelFaces[label][labelFaceCount[label]++] = face;
            }

            faceLabel.clear();
        }
    }

    /**
     * Applies a {@link SeqTransform} of the specified <code>id</code>.
     *
     * @param id the transform id.
     */
    void Model::ApplyTransform(int32_t id)
    {
        if (labelVertices.empty()) {
            return;
        }
        if (id == -1) {
            return;
        }
        const auto& transform = SeqTransform::Get(id);
        if (transform == nullptr) {
            return;
        }
        auto& skeleton = transform->skeleton;
        baseX = 0;
        baseY = 0;
        baseZ = 0;
        for (int32_t i = 0; i < transform->length; i++) {
            int32_t base = transform->bases[i];
            ApplyTransform(skeleton.baseTypes[base], skeleton.baseLabels[base], transform->x[i], transform->y[i], transform->z[i]);
        }
    }

    /**
     * Calculates {@link #minY}, {@link #maxY}, {@link #radius}, {@link #minDepth} and {@link #maxDepth}.
     */
    void Model::CalculateBoundsCylinder() {
        minY = 0;
        radius = 0;
        maxY = 0;
        for (int32_t i = 0; i < vertexCount; i++) {
            const int32_t x = vertexX[i];
            const int32_t y = vertexY[i];
            const int32_t z = vertexZ[i];
            if (-y > minY) {
                minY = -y;
            }
            if (y > maxY) {
                maxY = y;
            }
            const int32_t radiusSqr = (x * x) + (z * z);
            if (radiusSqr > radius) {
                radius = radiusSqr;
            }
        }
        radius = static_cast<int32_t>(std::sqrt(radius) + 0.99);
        minDepth = static_cast<int32_t>(std::sqrt((radius * radius) + (minY * minY)) + 0.99);
        maxDepth = minDepth + static_cast<int32_t>(std::sqrt((radius * radius) + (maxY * maxY)) + 0.99);
    }

    /**
     * Calculates {@link #minY}, {@link #maxY}, {@link #minDepth} and {@link #maxDepth}.
     */
    void Model::CalculateBoundsY()
    {
        minY = 0;
        maxY = 0;
        for (int32_t i = 0; i < vertexCount; i++) {
            int32_t y = vertexY[i];
            if (-y > minY) {
                minY = -y;
            }
            if (y > maxY) {
                maxY = y;
            }
        }
        minDepth = static_cast<int32_t>(std::sqrt((radius * radius) + (minY * minY)) + 0.99);
        maxDepth = minDepth + static_cast<int32_t>(std::sqrt((radius * radius) + (maxY * maxY)) + 0.99);
    }

    void Model::Init(const int32_t count, OnDemand* ondemand) {
        headers.resize(count);
        Model::ondemand = ondemand;
    }

    void Model::Unpack(const std::vector<int8_t> &src, const int32_t id) {
        if (src.empty()) {
            headers[id] = Header(); // create new Header
            Header& header = headers[id];
            header.vertexCount = 0;
            header.faceCount = 0;
            header.texturedFaceCount = 0;
            return;
        }

        Buffer buffer(src);
        buffer.position = src.size() - 18;
        headers[id] = Header(); // create new Header
        Header& header = headers[id];
        header.data = src;
        header.vertexCount = buffer.ReadU16();
        header.faceCount = buffer.ReadU16();
        header.texturedFaceCount = buffer.ReadU8();

        const int32_t hasInfo = buffer.ReadU8();
        const int32_t priority = buffer.ReadU8();
        const int32_t hasAlpha = buffer.ReadU8();
        const int32_t hasFaceLabels = buffer.ReadU8();
        const int32_t hasVertexLabels = buffer.ReadU8();

        const int32_t dataLengthX = buffer.ReadU16();
        const int32_t dataLengthY = buffer.ReadU16();
        const int32_t dataLengthZ = buffer.ReadU16();
        const int32_t dataLengthFaceOrientations = buffer.ReadU16();

        int32_t offset = 0;
        header.vertexFlagsOffset = offset;
        offset += header.vertexCount;

        header.faceOrientationsOffset = offset;
        offset += header.faceCount;

        header.facePrioritiesOffset = offset;
        if (priority == 255) {
            offset += header.faceCount;
        } else {
            header.facePrioritiesOffset = -priority - 1;
        }

        header.faceLabelsOffset = offset;
        if (hasFaceLabels == 1) {
            offset += header.faceCount;
        } else {
            header.faceLabelsOffset = -1;
        }

        header.faceInfosOffset = offset;
        if (hasInfo == 1) {
            offset += header.faceCount;
        } else {
            header.faceInfosOffset = -1;
        }

        header.vertexLabelsOffset = offset;
        if (hasVertexLabels == 1) {
            offset += header.vertexCount;
        } else {
            header.vertexLabelsOffset = -1;
        }

        header.faceAlphasOffset = offset;
        if (hasAlpha == 1) {
            offset += header.faceCount;
        } else {
            header.faceAlphasOffset = -1;
        }

        header.faceVerticesOffset = offset;
        offset += dataLengthFaceOrientations;

        header.faceColorsOffset = offset;
        offset += header.faceCount * 2;

        header.faceTextureAxisOffset = offset;
        offset += header.texturedFaceCount * 6;

        header.vertexXOffset = offset;
        offset += dataLengthX;
        header.vertexYOffset = offset;
        offset += dataLengthY;
        header.vertexZOffset = offset;
        offset += dataLengthZ;
        header.loaded = true;
    }

    std::shared_ptr<Model> Model::TryGet(int id) {

        if (headers.empty() || id < 0 || id >= static_cast<int>(headers.size())) {
            return nullptr;
        }

        Header& header = headers[id];

        if (header.loaded) {
            return std::make_shared<Model>(id);
        }
        ondemand->RequestModel(id);
        return nullptr;
    }

    /**
     * Utility function. Checks if <code>(x, y)</code> is within the provided triangle.
     *
     * @param x  the x.
     * @param y  the y.
     * @param yA y of corner a.
     * @param yB y of corner b.
     * @param yC y of corner c.
     * @param xA x of corner a.
     * @param xB x of corner b.
     * @param xC x of corner c.
     * @return <code>true</code> if <code>(x, y)</code> is within the triangle.
     */
    bool Model::PointWithinTriangle(int32_t x, int32_t y, int32_t yA, int32_t yB, int32_t yC, int32_t xA, int32_t xB,
        int32_t xC) {
        if ((y < yA) && (y < yB) && (y < yC)) {
            return false;
        }
        if ((y > yA) && (y > yB) && (y > yC)) {
            return false;
        }
        if ((x < xA) && (x < xB) && (x < xC)) {
            return false;
        }
        return (x <= xA) || (x <= xB) || (x <= xC);
    }

    /**
     * Utility function. Multiplies the input HSL lightness component by the provided <code>scalar</code>.
     *
     * @param hsl      the color value.
     * @param scalar   the scalar. [0...127]
     * @param faceInfo Provided face info to determine the type of color to return. Textured triangles (type 2) only have
     *                 a lightness component.
     * @return the color.
     * @see #palette
     */
    int32_t Model::MulColorLightness(int32_t hsl, int32_t scalar, int32_t faceInfo) {
        if ((faceInfo & 2) == 2) {
            if (scalar < 0) {
                scalar = 0;
            } else if (scalar > 127) {
                scalar = 127;
            }
            scalar = 127 - scalar;
            return scalar;
        }
        scalar = (scalar * (hsl & 0x7f)) >> 7;
        if (scalar < 2) {
            scalar = 2;
        } else if (scalar > 126) {
            scalar = 126;
        }
        return (hsl & 0xff80) + scalar;
    }
}
