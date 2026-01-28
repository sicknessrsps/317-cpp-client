#include "Draw3D.h"
#include "Draw2D.h"

namespace SDL_Client {

    /**
     * Instantiates {@link #lineOffset} and sets {@link #centerX} and {@link #centerY} using the width and height values
     * provided.
     *
     * @param width  the width.
     * @param height the height.
     */
    void Draw3D::Init3D(int32_t width, int32_t height) {
        lineOffset.resize(height);
        for (int32_t y = 0; y < height; ++y) {
            lineOffset[y] = width * y;
        }
        centerX = width / 2;
        centerY = height / 2;
    }

    /**
     * Instantiates {@link #lineOffset} and sets {@link #centerX} and {@link #centerY} using the width and height values
     * stored in {@link Draw2D}.
     */
    void Draw3D::Init2D() {
        lineOffset.resize(Draw2D::height);
        for (int32_t y = 0; y < Draw2D::height; y++) {
            lineOffset[y] = Draw2D::width * y;
        }
        centerX = Draw2D::width / 2;
        centerY = Draw2D::height / 2;
    }

    /**
     *
     * Rasterizes a filled triangle by drawing horizontal scanlines. It computes edge slopes (xStepAB, xStepBC, xStepAC) in 16.16 fixed-point (shifts by 16)
     * The function is long because it handles all permutations (which vertex is top/middle/bottom, flat-top vs flat-bottom cases, etc.).
     */
    void Draw3D::FillTriangle(int32_t y0, int32_t y1, int32_t y2, int32_t x0, int32_t x1, int32_t x2, int32_t color) {
        int32_t xStepAB = 0;
        if (y1 != y0) {
            xStepAB = ((x1 - x0) << 16) / (y1 - y0);
        }
        int xStepBC = 0;
        if (y2 != y1) {
            xStepBC = ((x2 - x1) << 16) / (y2 - y1);
        }
        int xStepAC = 0;
        if (y2 != y0) {
            xStepAC = ((x0 - x2) << 16) / (y0 - y2);
        }

        // Cache frequently accessed values
        const int32_t width = Draw2D::width;
        const int32_t bottom = Draw2D::bottom;
        int32_t* pixels = Draw2D::Pixels();

        if ((y0 <= y1) && (y0 <= y2)) {
            if (y0 >= bottom) {
                return;
            }
            if (y1 > bottom) {
                y1 = bottom;
            }
            if (y2 > bottom) {
                y2 = bottom;
            }
            if (y1 < y2) {
                x2 = (x0 <<= 16);
                if (y0 < 0) {
                    x2 -= xStepAC * y0;
                    x0 -= xStepAB * y0;
                    y0 = 0;
                }
                x1 <<= 16;
                if (y1 < 0) {
                    x1 -= xStepBC * y1;
                    y1 = 0;
                }
                if (((y0 != y1) && (xStepAC < xStepAB)) || ((y0 == y1) && (xStepAC > xStepBC))) {
                    y2 -= y1;
                    y1 -= y0;
                    for (y0 = lineOffset[y0]; --y1 >= 0; y0 += width) {
                        DrawScanline(pixels, y0, color, x2 >> 16, x0 >> 16);
                        x2 += xStepAC;
                        x0 += xStepAB;
                    }
                    while (--y2 >= 0) {
                        DrawScanline(pixels, y0, color, x2 >> 16, x1 >> 16);
                        x2 += xStepAC;
                        x1 += xStepBC;
                        y0 += width;
                    }
                    return;
                }
                y2 -= y1;
                y1 -= y0;
                for (y0 = lineOffset[y0]; --y1 >= 0; y0 += width) {
                    DrawScanline(pixels, y0, color, x0 >> 16, x2 >> 16);
                    x2 += xStepAC;
                    x0 += xStepAB;
                }
                while (--y2 >= 0) {
                    DrawScanline(pixels, y0, color, x1 >> 16, x2 >> 16);
                    x2 += xStepAC;
                    x1 += xStepBC;
                    y0 += width;
                }
                return;
            }
            x1 = (x0 <<= 16);
            if (y0 < 0) {
                x1 -= xStepAC * y0;
                x0 -= xStepAB * y0;
                y0 = 0;
            }
            x2 <<= 16;
            if (y2 < 0) {
                x2 -= xStepBC * y2;
                y2 = 0;
            }
            if (((y0 != y2) && (xStepAC < xStepAB)) || ((y0 == y2) && (xStepBC > xStepAB))) {
                y1 -= y2;
                y2 -= y0;
                for (y0 = lineOffset[y0]; --y2 >= 0; y0 += width) {
                    DrawScanline(pixels, y0, color, x1 >> 16, x0 >> 16);
                    x1 += xStepAC;
                    x0 += xStepAB;
                }
                while (--y1 >= 0) {
                    DrawScanline(pixels, y0, color, x2 >> 16, x0 >> 16);
                    x2 += xStepBC;
                    x0 += xStepAB;
                    y0 += width;
                }
                return;
            }
            y1 -= y2;
            y2 -= y0;
            for (y0 = lineOffset[y0]; --y2 >= 0; y0 += width) {
                DrawScanline(pixels, y0, color, x0 >> 16, x1 >> 16);
                x1 += xStepAC;
                x0 += xStepAB;
            }
            while (--y1 >= 0) {
                DrawScanline(pixels, y0, color, x0 >> 16, x2 >> 16);
                x2 += xStepBC;
                x0 += xStepAB;
                y0 += width;
            }
            return;
        }
        if (y1 <= y2) {
            if (y1 >= bottom) {
                return;
            }
            if (y2 > bottom) {
                y2 = bottom;
            }
            if (y0 > bottom) {
                y0 = bottom;
            }
            if (y2 < y0) {
                x0 = (x1 <<= 16);
                if (y1 < 0) {
                    x0 -= xStepAB * y1;
                    x1 -= xStepBC * y1;
                    y1 = 0;
                }
                x2 <<= 16;
                if (y2 < 0) {
                    x2 -= xStepAC * y2;
                    y2 = 0;
                }
                if (((y1 != y2) && (xStepAB < xStepBC)) || ((y1 == y2) && (xStepAB > xStepAC))) {
                    y0 -= y2;
                    y2 -= y1;
                    for (y1 = lineOffset[y1]; --y2 >= 0; y1 += width) {
                        DrawScanline(pixels, y1, color, x0 >> 16, x1 >> 16);
                        x0 += xStepAB;
                        x1 += xStepBC;
                    }
                    while (--y0 >= 0) {
                        DrawScanline(pixels, y1, color, x0 >> 16, x2 >> 16);
                        x0 += xStepAB;
                        x2 += xStepAC;
                        y1 += width;
                    }
                    return;
                }
                y0 -= y2;
                y2 -= y1;
                for (y1 = lineOffset[y1]; --y2 >= 0; y1 += width) {
                    DrawScanline(pixels, y1, color, x1 >> 16, x0 >> 16);
                    x0 += xStepAB;
                    x1 += xStepBC;
                }
                while (--y0 >= 0) {
                    DrawScanline(pixels, y1, color, x2 >> 16, x0 >> 16);
                    x0 += xStepAB;
                    x2 += xStepAC;
                    y1 += width;
                }
                return;
            }
            x2 = (x1 <<= 16);
            if (y1 < 0) {
                x2 -= xStepAB * y1;
                x1 -= xStepBC * y1;
                y1 = 0;
            }
            x0 <<= 16;
            if (y0 < 0) {
                x0 -= xStepAC * y0;
                y0 = 0;
            }
            if (xStepAB < xStepBC) {
                y2 -= y0;
                y0 -= y1;
                for (y1 = lineOffset[y1]; --y0 >= 0; y1 += width) {
                    DrawScanline(pixels, y1, color, x2 >> 16, x1 >> 16);
                    x2 += xStepAB;
                    x1 += xStepBC;
                }
                while (--y2 >= 0) {
                    DrawScanline(pixels, y1, color, x0 >> 16, x1 >> 16);
                    x0 += xStepAC;
                    x1 += xStepBC;
                    y1 += width;
                }
                return;
            }
            y2 -= y0;
            y0 -= y1;
            for (y1 = lineOffset[y1]; --y0 >= 0; y1 += width) {
                DrawScanline(pixels, y1, color, x1 >> 16, x2 >> 16);
                x2 += xStepAB;
                x1 += xStepBC;
            }
            while (--y2 >= 0) {
                DrawScanline(pixels, y1, color, x1 >> 16, x0 >> 16);
                x0 += xStepAC;
                x1 += xStepBC;
                y1 += width;
            }
            return;
        }
        if (y2 >= bottom) {
            return;
        }
        if (y0 > bottom) {
            y0 = bottom;
        }
        if (y1 > bottom) {
            y1 = bottom;
        }
        if (y0 < y1) {
            x1 = (x2 <<= 16);
            if (y2 < 0) {
                x1 -= xStepBC * y2;
                x2 -= xStepAC * y2;
                y2 = 0;
            }
            x0 <<= 16;
            if (y0 < 0) {
                x0 -= xStepAB * y0;
                y0 = 0;
            }
            if (xStepBC < xStepAC) {
                y1 -= y0;
                y0 -= y2;
                for (y2 = lineOffset[y2]; --y0 >= 0; y2 += width) {
                    DrawScanline(pixels, y2, color, x1 >> 16, x2 >> 16);
                    x1 += xStepBC;
                    x2 += xStepAC;
                }
                while (--y1 >= 0) {
                    DrawScanline(pixels, y2, color, x1 >> 16, x0 >> 16);
                    x1 += xStepBC;
                    x0 += xStepAB;
                    y2 += width;
                }
                return;
            }
            y1 -= y0;
            y0 -= y2;
            for (y2 = lineOffset[y2]; --y0 >= 0; y2 += width) {
                DrawScanline(pixels, y2, color, x2 >> 16, x1 >> 16);
                x1 += xStepBC;
                x2 += xStepAC;
            }
            while (--y1 >= 0) {
                DrawScanline(pixels, y2, color, x0 >> 16, x1 >> 16);
                x1 += xStepBC;
                x0 += xStepAB;
                y2 += width;
            }
            return;
        }
        x0 = (x2 <<= 16);
        if (y2 < 0) {
            x0 -= xStepBC * y2;
            x2 -= xStepAC * y2;
            y2 = 0;
        }
        x1 <<= 16;
        if (y1 < 0) {
            x1 -= xStepAB * y1;
            y1 = 0;
        }
        if (xStepBC < xStepAC) {
            y0 -= y1;
            y1 -= y2;
            for (y2 = lineOffset[y2]; --y1 >= 0; y2 += width) {
                DrawScanline(pixels, y2, color, x0 >> 16, x2 >> 16);
                x0 += xStepBC;
                x2 += xStepAC;
            }
            while (--y0 >= 0) {
                DrawScanline(pixels, y2, color, x1 >> 16, x2 >> 16);
                x1 += xStepAB;
                x2 += xStepAC;
                y2 += width;
            }
            return;
        }
        y0 -= y1;
        y1 -= y2;
        for (y2 = lineOffset[y2]; --y1 >= 0; y2 += width) {
            DrawScanline(pixels, y2, color, x2 >> 16, x0 >> 16);
            x0 += xStepBC;
            x2 += xStepAC;
        }
        while (--y0 >= 0) {
            DrawScanline(pixels, y2, color, x2 >> 16, x1 >> 16);
            x1 += xStepAB;
            x2 += xStepAC;
            y2 += width;
        }
    }

    /**
     * Fills a gouraud triangle using {@link #palette} colors.
     *
     * @param yA     the Y for corner A.
     * @param yB     the Y for corner B.
     * @param yC     the Y for corner C.
     * @param xA     the X for corner A.
     * @param xB     the X for corner B.
     * @param xC     the X for corner C.
     * @param colorA the color for corner A.
     * @param colorB the color for corner B.
     * @param colorC the color for corner C.
     */
    void Draw3D::FillGouraudTriangle(int32_t yA, int32_t yB, int32_t yC, int32_t xA, int32_t xB, int32_t xC,
        int32_t colorA, int32_t colorB, int32_t colorC) {
        int32_t xStepAB = 0;
        int32_t xStepBC = 0;
        int32_t xStepAC = 0;

        int32_t colorStepAB = 0;
        int32_t colorStepBC = 0;
        int32_t colorStepAC = 0;

        if (yB != yA) {
            xStepAB = ((xB - xA) << 16) / (yB - yA);
            colorStepAB = ((colorB - colorA) << 15) / (yB - yA);
        }

        if (yC != yB) {
            xStepBC = ((xC - xB) << 16) / (yC - yB);
            colorStepBC = ((colorC - colorB) << 15) / (yC - yB);
        }

        if (yC != yA) {
            xStepAC = ((xA - xC) << 16) / (yA - yC);
            colorStepAC = ((colorA - colorC) << 15) / (yA - yC);
        }

        if ((yA <= yB) && (yA <= yC)) {
            if (yA >= Draw2D::bottom) {
                return;
            }
            if (yB > Draw2D::bottom) {
                yB = Draw2D::bottom;
            }
            if (yC > Draw2D::bottom) {
                yC = Draw2D::bottom;
            }
            if (yB < yC) {
                xC = (xA <<= 16);
                colorC = (colorA <<= 15);
                if (yA < 0) {
                    xC -= xStepAC * yA;
                    xA -= xStepAB * yA;
                    colorC -= colorStepAC * yA;
                    colorA -= colorStepAB * yA;
                    yA = 0;
                }
                xB <<= 16;
                colorB <<= 15;
                if (yB < 0) {
                    xB -= xStepBC * yB;
                    colorB -= colorStepBC * yB;
                    yB = 0;
                }
                if (((yA != yB) && (xStepAC < xStepAB)) || ((yA == yB) && (xStepAC > xStepBC))) {
                    yC -= yB;
                    yB -= yA;
                    for (yA = lineOffset[yA]; --yB >= 0; yA += Draw2D::width) {
                        DrawGouraudScanline(Draw2D::Pixels(), yA, xC >> 16, xA >> 16, colorC >> 7, colorA >> 7);
                        xC += xStepAC;
                        xA += xStepAB;
                        colorC += colorStepAC;
                        colorA += colorStepAB;
                    }
                    while (--yC >= 0) {
                        DrawGouraudScanline(Draw2D::Pixels(), yA, xC >> 16, xB >> 16, colorC >> 7, colorB >> 7);
                        xC += xStepAC;
                        xB += xStepBC;
                        colorC += colorStepAC;
                        colorB += colorStepBC;
                        yA += Draw2D::width;
                    }
                    return;
                }
                yC -= yB;
                yB -= yA;
                for (yA = lineOffset[yA]; --yB >= 0; yA += Draw2D::width) {
                    DrawGouraudScanline(Draw2D::Pixels(), yA, xA >> 16, xC >> 16, colorA >> 7, colorC >> 7);
                    xC += xStepAC;
                    xA += xStepAB;
                    colorC += colorStepAC;
                    colorA += colorStepAB;
                }
                while (--yC >= 0) {
                    DrawGouraudScanline(Draw2D::Pixels(), yA, xB >> 16, xC >> 16, colorB >> 7, colorC >> 7);
                    xC += xStepAC;
                    xB += xStepBC;
                    colorC += colorStepAC;
                    colorB += colorStepBC;
                    yA += Draw2D::width;
                }
                return;
            }
            xB = (xA <<= 16);
            colorB = (colorA <<= 15);
            if (yA < 0) {
                xB -= xStepAC * yA;
                xA -= xStepAB * yA;
                colorB -= colorStepAC * yA;
                colorA -= colorStepAB * yA;
                yA = 0;
            }
            xC <<= 16;
            colorC <<= 15;
            if (yC < 0) {
                xC -= xStepBC * yC;
                colorC -= colorStepBC * yC;
                yC = 0;
            }
            if (((yA != yC) && (xStepAC < xStepAB)) || ((yA == yC) && (xStepBC > xStepAB))) {
                yB -= yC;
                yC -= yA;
                for (yA = lineOffset[yA]; --yC >= 0; yA += Draw2D::width) {
                    DrawGouraudScanline(Draw2D::Pixels(), yA, xB >> 16, xA >> 16, colorB >> 7, colorA >> 7);
                    xB += xStepAC;
                    xA += xStepAB;
                    colorB += colorStepAC;
                    colorA += colorStepAB;
                }
                while (--yB >= 0) {
                    DrawGouraudScanline(Draw2D::Pixels(), yA, xC >> 16, xA >> 16, colorC >> 7, colorA >> 7);
                    xC += xStepBC;
                    xA += xStepAB;
                    colorC += colorStepBC;
                    colorA += colorStepAB;
                    yA += Draw2D::width;
                }
                return;
            }
            yB -= yC;
            yC -= yA;
            for (yA = lineOffset[yA]; --yC >= 0; yA += Draw2D::width) {
                DrawGouraudScanline(Draw2D::Pixels(), yA, xA >> 16, xB >> 16, colorA >> 7, colorB >> 7);
                xB += xStepAC;
                xA += xStepAB;
                colorB += colorStepAC;
                colorA += colorStepAB;
            }
            while (--yB >= 0) {
                DrawGouraudScanline(Draw2D::Pixels(), yA, xA >> 16, xC >> 16, colorA >> 7, colorC >> 7);
                xC += xStepBC;
                xA += xStepAB;
                colorC += colorStepBC;
                colorA += colorStepAB;
                yA += Draw2D::width;
            }
            return;
        }
        if (yB <= yC) {
            if (yB >= Draw2D::bottom) {
                return;
            }
            if (yC > Draw2D::bottom) {
                yC = Draw2D::bottom;
            }
            if (yA > Draw2D::bottom) {
                yA = Draw2D::bottom;
            }
            if (yC < yA) {
                xA = (xB <<= 16);
                colorA = (colorB <<= 15);
                if (yB < 0) {
                    xA -= xStepAB * yB;
                    xB -= xStepBC * yB;
                    colorA -= colorStepAB * yB;
                    colorB -= colorStepBC * yB;
                    yB = 0;
                }
                xC <<= 16;
                colorC <<= 15;
                if (yC < 0) {
                    xC -= xStepAC * yC;
                    colorC -= colorStepAC * yC;
                    yC = 0;
                }
                if (((yB != yC) && (xStepAB < xStepBC)) || ((yB == yC) && (xStepAB > xStepAC))) {
                    yA -= yC;
                    yC -= yB;
                    for (yB = lineOffset[yB]; --yC >= 0; yB += Draw2D::width) {
                        DrawGouraudScanline(Draw2D::Pixels(), yB, xA >> 16, xB >> 16, colorA >> 7, colorB >> 7);
                        xA += xStepAB;
                        xB += xStepBC;
                        colorA += colorStepAB;
                        colorB += colorStepBC;
                    }
                    while (--yA >= 0) {
                        DrawGouraudScanline(Draw2D::Pixels(), yB, xA >> 16, xC >> 16, colorA >> 7, colorC >> 7);
                        xA += xStepAB;
                        xC += xStepAC;
                        colorA += colorStepAB;
                        colorC += colorStepAC;
                        yB += Draw2D::width;
                    }
                    return;
                }
                yA -= yC;
                yC -= yB;
                for (yB = lineOffset[yB]; --yC >= 0; yB += Draw2D::width) {
                    DrawGouraudScanline(Draw2D::Pixels(), yB, xB >> 16, xA >> 16, colorB >> 7, colorA >> 7);
                    xA += xStepAB;
                    xB += xStepBC;
                    colorA += colorStepAB;
                    colorB += colorStepBC;
                }
                while (--yA >= 0) {
                    DrawGouraudScanline(Draw2D::Pixels(), yB, xC >> 16, xA >> 16, colorC >> 7, colorA >> 7);
                    xA += xStepAB;
                    xC += xStepAC;
                    colorA += colorStepAB;
                    colorC += colorStepAC;
                    yB += Draw2D::width;
                }
                return;
            }
            xC = (xB <<= 16);
            colorC = (colorB <<= 15);
            if (yB < 0) {
                xC -= xStepAB * yB;
                xB -= xStepBC * yB;
                colorC -= colorStepAB * yB;
                colorB -= colorStepBC * yB;
                yB = 0;
            }
            xA <<= 16;
            colorA <<= 15;
            if (yA < 0) {
                xA -= xStepAC * yA;
                colorA -= colorStepAC * yA;
                yA = 0;
            }
            if (xStepAB < xStepBC) {
                yC -= yA;
                yA -= yB;
                for (yB = lineOffset[yB]; --yA >= 0; yB += Draw2D::width) {
                    DrawGouraudScanline(Draw2D::Pixels(), yB, xC >> 16, xB >> 16, colorC >> 7, colorB >> 7);
                    xC += xStepAB;
                    xB += xStepBC;
                    colorC += colorStepAB;
                    colorB += colorStepBC;
                }
                while (--yC >= 0) {
                    DrawGouraudScanline(Draw2D::Pixels(), yB, xA >> 16, xB >> 16, colorA >> 7, colorB >> 7);
                    xA += xStepAC;
                    xB += xStepBC;
                    colorA += colorStepAC;
                    colorB += colorStepBC;
                    yB += Draw2D::width;
                }
                return;
            }
            yC -= yA;
            yA -= yB;
            for (yB = lineOffset[yB]; --yA >= 0; yB += Draw2D::width) {
                DrawGouraudScanline(Draw2D::Pixels(), yB, xB >> 16, xC >> 16, colorB >> 7, colorC >> 7);
                xC += xStepAB;
                xB += xStepBC;
                colorC += colorStepAB;
                colorB += colorStepBC;
            }
            while (--yC >= 0) {
                DrawGouraudScanline(Draw2D::Pixels(), yB, xB >> 16, xA >> 16, colorB >> 7, colorA >> 7);
                xA += xStepAC;
                xB += xStepBC;
                colorA += colorStepAC;
                colorB += colorStepBC;
                yB += Draw2D::width;
            }
            return;
        }
        if (yC >= Draw2D::bottom) {
            return;
        }
        if (yA > Draw2D::bottom) {
            yA = Draw2D::bottom;
        }
        if (yB > Draw2D::bottom) {
            yB = Draw2D::bottom;
        }
        if (yA < yB) {
            xB = (xC <<= 16);
            colorB = (colorC <<= 15);
            if (yC < 0) {
                xB -= xStepBC * yC;
                xC -= xStepAC * yC;
                colorB -= colorStepBC * yC;
                colorC -= colorStepAC * yC;
                yC = 0;
            }
            xA <<= 16;
            colorA <<= 15;
            if (yA < 0) {
                xA -= xStepAB * yA;
                colorA -= colorStepAB * yA;
                yA = 0;
            }
            if (xStepBC < xStepAC) {
                yB -= yA;
                yA -= yC;
                for (yC = lineOffset[yC]; --yA >= 0; yC += Draw2D::width) {
                    DrawGouraudScanline(Draw2D::Pixels(), yC, xB >> 16, xC >> 16, colorB >> 7, colorC >> 7);
                    xB += xStepBC;
                    xC += xStepAC;
                    colorB += colorStepBC;
                    colorC += colorStepAC;
                }
                while (--yB >= 0) {
                    DrawGouraudScanline(Draw2D::Pixels(), yC, xB >> 16, xA >> 16, colorB >> 7, colorA >> 7);
                    xB += xStepBC;
                    xA += xStepAB;
                    colorB += colorStepBC;
                    colorA += colorStepAB;
                    yC += Draw2D::width;
                }
                return;
            }
            yB -= yA;
            yA -= yC;
            for (yC = lineOffset[yC]; --yA >= 0; yC += Draw2D::width) {
                DrawGouraudScanline(Draw2D::Pixels(), yC, xC >> 16, xB >> 16, colorC >> 7, colorB >> 7);
                xB += xStepBC;
                xC += xStepAC;
                colorB += colorStepBC;
                colorC += colorStepAC;
            }
            while (--yB >= 0) {
                DrawGouraudScanline(Draw2D::Pixels(), yC, xA >> 16, xB >> 16, colorA >> 7, colorB >> 7);
                xB += xStepBC;
                xA += xStepAB;
                colorB += colorStepBC;
                colorA += colorStepAB;
                yC += Draw2D::width;
            }
            return;
        }
        xA = (xC <<= 16);
        colorA = (colorC <<= 15);
        if (yC < 0) {
            xA -= xStepBC * yC;
            xC -= xStepAC * yC;
            colorA -= colorStepBC * yC;
            colorC -= colorStepAC * yC;
            yC = 0;
        }
        xB <<= 16;
        colorB <<= 15;
        if (yB < 0) {
            xB -= xStepAB * yB;
            colorB -= colorStepAB * yB;
            yB = 0;
        }
        if (xStepBC < xStepAC) {
            yA -= yB;
            yB -= yC;
            for (yC = lineOffset[yC]; --yB >= 0; yC += Draw2D::width) {
                DrawGouraudScanline(Draw2D::Pixels(), yC, xA >> 16, xC >> 16, colorA >> 7, colorC >> 7);
                xA += xStepBC;
                xC += xStepAC;
                colorA += colorStepBC;
                colorC += colorStepAC;
            }
            while (--yA >= 0) {
                DrawGouraudScanline(Draw2D::Pixels(), yC, xB >> 16, xC >> 16, colorB >> 7, colorC >> 7);
                xB += xStepAB;
                xC += xStepAC;
                colorB += colorStepAB;
                colorC += colorStepAC;
                yC += Draw2D::width;
            }
            return;
        }
        yA -= yB;
        yB -= yC;
        for (yC = lineOffset[yC]; --yB >= 0; yC += Draw2D::width) {
            DrawGouraudScanline(Draw2D::Pixels(), yC, xC >> 16, xA >> 16, colorC >> 7, colorA >> 7);
            xA += xStepBC;
            xC += xStepAC;
            colorA += colorStepBC;
            colorC += colorStepAC;
        }
        while (--yA >= 0) {
            DrawGouraudScanline(Draw2D::Pixels(), yC, xC >> 16, xB >> 16, colorC >> 7, colorB >> 7);
            xB += xStepAB;
            xC += xStepAC;
            colorB += colorStepAB;
            colorC += colorStepAC;
            yC += Draw2D::width;
        }
    }

    void Draw3D::SetBrightness(const double brightness) {
        int32_t offset = 0;
        for (int32_t y = 0; y < 512; y++) {
            double hue = (static_cast<double>(y / 8) / 64) + 0.0078125;
            double saturation = (static_cast<double>(y & 7) / 8) + 0.0625;

            for (int32_t x = 0; x < 128; x++) {
                double lightness = static_cast<double>(x) / 128;
                double r = lightness;
                double g = lightness;
                double b = lightness;

                if (saturation != 0.0) {
                    double q;

                    if (lightness < 0.5) {
                        q = lightness * (1.0 + saturation);
                    } else {
                        q = (lightness + saturation) - (lightness * saturation);
                    }

                    double p = (2 * lightness) - q;
                    double t = hue + (1.0 / 3.0);

                    if (t > 1.0) {
                        t--;
                    }

                    double d11 = hue - (1.0 / 3.0);

                    if (d11 < 0.0) {
                        d11++;
                    }

                    if ((6 * t) < 1.0) {
                        r = p + ((q - p) * 6 * t);
                    } else if ((2 * t) < 1.0) {
                        r = q;
                    } else if ((3 * t) < 2) {
                        r = p + ((q - p) * ((2.0 / 3.0) - t) * 6);
                    } else {
                        r = p;
                    }

                    if ((6 * hue) < 1.0) {
                        g = p + ((q - p) * 6 * hue);
                    } else if ((2 * hue) < 1.0) {
                        g = q;
                    } else if ((3 * hue) < 2) {
                        g = p + ((q - p) * ((2.0 / 3.0) - hue) * 6);
                    } else {
                        g = p;
                    }

                    if ((6 * d11) < 1.0) {
                        b = p + ((q - p) * 6 * d11);
                    } else if ((2 * d11) < 1.0) {
                        b = q;
                    } else if ((3 * d11) < 2) {
                        b = p + ((q - p) * ((2.0 / 3.0) - d11) * 6);
                    } else {
                        b = p;
                    }
                }

                int32_t intR = static_cast<int>(r * 256);
                int32_t intG = static_cast<int>(g * 256);
                int32_t intB = static_cast<int>(b * 256);
                int32_t rgb = (intR << 16) + (intG << 8) + intB;

                rgb = SetGamma(rgb, brightness);

                if (rgb == 0) {
                    rgb = 1;
                }

                palette[offset++] = rgb;
            }
        }

        for (int32_t textureID = 0; textureID < 50; textureID++) {
            if (textures[textureID].width < 1) {
                continue;
            }

            std::vector<int32_t>& palette = textures[textureID].palette;
            texturePalette[textureID].resize(palette.size());

            for (int32_t i = 0; i < palette.size(); i++) {
                texturePalette[textureID][i] = SetGamma(palette[i], brightness);

                if (((texturePalette[textureID][i] & 0xf8f8ff) == 0) && (i != 0)) {
                    texturePalette[textureID][i] = 1;
                }
            }
        }

        for (int32_t textureID = 0; textureID < 50; textureID++) {
            PushTexture(textureID);
        }
    }

    /**
     * Initializes the texel pool.
     *
     * @param poolSize the initial pool size.
     */
    void Draw3D::InitPool(int32_t poolSize)
    {
        if (texelPool.empty()) {
            Draw3D::poolSize = poolSize;
            if (lowmem) {
                texelPool = std::vector(Draw3D::poolSize, std::vector<int32_t>(64 * 64 * 4));
            } else {
                texelPool = std::vector(Draw3D::poolSize, std::vector<int32_t>(128 * 128 * 4));
            }
            for (int32_t k = 0; k < 50; k++) {
                activeTexels[k].clear();
            }
        }
    }

    /**
     * Nullifies the texel pool. {@link #initPool(int)} must be called again if textured triangles are drawn.
     */
    void Draw3D::ClearTexels()
    {
        texelPool.clear();
        for (int32_t j = 0; j < 50; j++) {
            activeTexels[j].clear();
        }
    }

    void Draw3D::UnpackTextures(FileArchive& archive) {
        textureCount = 0;
        for (int32_t textureID = 0; textureID < 50; textureID++) {
            textures[textureID] = Image8(archive, std::to_string(textureID), 0);

            if (lowmem && (textures[textureID].cropW == 128)) {
                textures[textureID].Shrink();
            } else {
                textures[textureID].Crop();
            }
            textureCount++;
        }
    }

    std::vector<int32_t>& Draw3D::GetTexels(int32_t textureID)
    {
        textureCycle[textureID] = cycle++;

        // Already built? Return it
        if (!activeTexels[textureID].empty()) {
            return activeTexels[textureID];
        }

        // Allocate space for full 128x128 * 4 shades = 65536 ints
        std::vector texels(16384 * 4, 0);

        if (poolSize > 0) {
            texels = texelPool[--poolSize];
            texelPool[poolSize].clear();
        } else {
            int32_t tCycle = 0;
            int32_t selected = -1;

            for (int32_t t = 0; t < textureCount; t++) {
                if ((!activeTexels[t].empty()) && ((textureCycle[t] < tCycle) || (selected == -1))) {
                    tCycle = textureCycle[t];
                    selected = t;
                }
            }

            if (selected != -1) {
                texels = activeTexels[selected];
                activeTexels[selected].clear();
            }
        }

        activeTexels[textureID] = texels;
        Image8& texture = textures[textureID];
        std::vector<int32_t>& tPalette = texturePalette[textureID];

        if (lowmem)
        {
            textureTranslucent[textureID] = false;

            for (int32_t i = 0; i < 4096; i++) {
                int32_t rgb = texels[i] = palette[texture.pixels[i]] & 0xf8f8ff;

                if (rgb == 0) {
                    textureTranslucent[textureID] = true;
                }

                texels[4096 + i] = (rgb - (rgb >> 3)) & 0xF8F8FF;
                texels[8192 + i] = (rgb - (rgb >> 2)) & 0xF8F8FF;
                texels[12288 + i] = (rgb - (rgb >> 2) - (rgb >> 3)) & 0xF8F8FF;
            }
        } else
        {
            // Upscale 64x64 → 128x128 or copy directly
            if (texture.width == 64) {
                for (int32_t y = 0; y < 128; ++y) {
                    for (int32_t x = 0; x < 128; ++x) {
                        texels[x + (y << 7)] =
                            tPalette[texture.pixels[(x >> 1) + ((y >> 1) << 6)]];
                    }
                }
            } else {
                for (int32_t i = 0; i < 16384; ++i) {
                    texels[i] = tPalette[texture.pixels[i]];
                }
            }

            textureTranslucent[textureID] = false;

            // Build brightness variants (same as Java)
            for (int32_t i = 0; i < 16384; ++i) {
                texels[i] &= 0xF8F8FF;

                int32_t rgb = texels[i];
                if (rgb == 0) textureTranslucent[textureID] = true;

                texels[16384 + i] = (rgb - (rgb >> 3)) & 0xF8F8FF;
                texels[32768 + i] = (rgb - (rgb >> 2)) & 0xF8F8FF;
                texels[49152 + i] = (rgb - (rgb >> 2) - (rgb >> 3)) & 0xF8F8FF;
            }

            // Store it back (move to avoid copy)
        }
        activeTexels[textureID] = std::move(texels);
        return activeTexels[textureID];
    }

    int32_t Draw3D::GetAverageTextureRGB(int32_t textureID) {
        if (averageTextureRGB[textureID] != 0) {
            return averageTextureRGB[textureID];
        }
        int32_t r = 0;
        int32_t g = 0;
        int32_t b = 0;
        const auto length = static_cast<int32_t>(texturePalette[textureID].size());
        for (int32_t i = 0; i < length; i++) {
            r += (texturePalette[textureID][i] >> 16) & 0xff;
            g += (texturePalette[textureID][i] >> 8) & 0xff;
            b += texturePalette[textureID][i] & 0xff;
        }
        int32_t rgb = ((r / length) << 16) + ((g / length) << 8) + (b / length);
        rgb = SetGamma(rgb, 1.4);
        if (rgb == 0) {
            rgb = 1;
        }
        averageTextureRGB[textureID] = rgb;
        return rgb;
    }

    /**
     * Returns the <code>rgb</code> with each component raised to the power of <code>gamma</code>
     *
     * @param rgb   the input rgb.
     * @param gamma the gamma.
     * @return the result.
     */
    int32_t Draw3D::SetGamma(int32_t rgb, double gamma) {
        double r = static_cast<double>((rgb >> 16) & 0xFF) / 256.0;
        double g = static_cast<double>((rgb >> 8) & 0xFF) / 256.0;
        double b = static_cast<double>(rgb & 0xFF) / 256.0;

        r = std::pow(r, gamma);
        g = std::pow(g, gamma);
        b = std::pow(b, gamma);

        const int32_t intR = static_cast<int32_t>(r * 256.0);
        const int32_t intG = static_cast<int32_t>(g * 256.0);
        const int32_t intB = static_cast<int32_t>(b * 256.0);

        return (intR << 16) | (intG << 8) | intB;
    }

    /**
     * Pushes the texels of the provided texture id back into the pool. This causes the texture to be regenerated the
     * next time {@link #getTexels(int)} is called for that <code>textureID</code>. This method is actively used for
     * scrolling textures by {@link Game#updateTextures(int)}
     *
     * @param textureID the texture id.
     */
    void Draw3D::PushTexture(int32_t textureID)
    {
        if (!activeTexels[textureID].empty()) {
            texelPool[poolSize++] = activeTexels[textureID];
            activeTexels[textureID].clear();
        }
    }

    void Draw3D::Unload()
    {
        //sin = null;
        //cos = null;
        lineOffset.clear();
        for (auto& tex : textures) tex = Image8();
        textureTranslucent.fill(0);
        averageTextureRGB.fill(0);
        texelPool.clear();
        for (auto& texels : activeTexels) texels.clear();
        textureCycle.fill(0);
        //palette
        for (auto& palette : texturePalette) palette.clear();
    }

    /**
     * Draws a gouraud scanline using {@link #palette} colors.
     *
     * @param dst    the destination.
     * @param offset the destination offset.
     * @param x0     the left x.
     * @param x1     the right x.
     * @param color0 the left color index.
     * @param color1 the right color index.
     * @see #fillGouraudTriangle(int, int, int, int, int, int, int, int, int) e
     */
    void Draw3D::DrawGouraudScanline(int32_t *dst, int32_t offset, int32_t x0, int32_t x1, int32_t color0,
        int32_t color1) {
        int32_t rgb;
        int32_t length;

        ASSERT((color0 >> 8) >= 0 && (color0 >> 8) < 0x10000);

        if (jagged) {
            int32_t colorStep;

            if (clipX) {
                if ((x1 - x0) > 3) {
                    colorStep = (color1 - color0) / (x1 - x0);
                } else {
                    colorStep = 0;
                }

                if (x1 > Draw2D::boundX) {
                    x1 = Draw2D::boundX;
                }

                if (x0 < 0) {
                    color0 -= x0 * colorStep;
                    x0 = 0;
                }

                if (x0 >= x1) {
                    return;
                }

                offset += x0;
                length = (x1 - x0) >> 2;
                colorStep <<= 2;
            } else {
                if (x0 >= x1) {
                    return;
                }

                offset += x0;
                length = (x1 - x0) >> 2;

                if (length > 0) {
                    colorStep = ((color1 - color0) * reciprocal15[length]) >> 15;
                } else {
                    colorStep = 0;
                }
            }

            if (Draw3D::alpha == 0) {
                const int32_t alphaPrefix = 0xFF000000;
                const int32_t* palettePtr = palette.data();

                while (--length >= 0) {
                    rgb = palettePtr[color0 >> 8];
                    color0 += colorStep;
                    const int32_t colorWithAlpha = alphaPrefix | rgb;
                    dst[offset++] = colorWithAlpha;
                    dst[offset++] = colorWithAlpha;
                    dst[offset++] = colorWithAlpha;
                    dst[offset++] = colorWithAlpha;
                }

                length = (x1 - x0) & 3;

                if (length > 0) {
                    rgb = palettePtr[color0 >> 8];
                    const int32_t colorWithAlpha = alphaPrefix | rgb;
                    do {
                        dst[offset++] = colorWithAlpha;
                    } while (--length > 0);
                    return;
                }
            } else {
                const int32_t alpha = Draw3D::alpha;
                const int32_t invAlpha = 256 - alpha;
                const int32_t alphaPrefix = 0xFF000000;
                const int32_t* palettePtr = palette.data();

                while (--length >= 0) {
                    rgb = palettePtr[color0 >> 8];
                    color0 += colorStep;
                    rgb = ((((rgb & 0xff00ff) * invAlpha) >> 8) & 0xff00ff) + ((((rgb & 0xff00) * invAlpha) >> 8) & 0xff00);
                    dst[offset++] = alphaPrefix | rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                    dst[offset++] = alphaPrefix | rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                    dst[offset++] = alphaPrefix | rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                    dst[offset++] = alphaPrefix | rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                }

                length = (x1 - x0) & 3;

                if (length > 0) {
                    rgb = palettePtr[color0 >> 8];
                    rgb = ((((rgb & 0xff00ff) * invAlpha) >> 8) & 0xff00ff) + ((((rgb & 0xff00) * invAlpha) >> 8) & 0xff00);
                    do {
                        dst[offset++] = alphaPrefix | rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00);
                    } while (--length > 0);
                }
            }
            return;
        }
        if (x0 >= x1) {
            return;
        }

        int32_t colorStep = (color1 - color0) / (x1 - x0);

        if (clipX) {
            if (x1 > Draw2D::boundX) {
                x1 = Draw2D::boundX;
            }
            if (x0 < 0) {
                color0 -= x0 * colorStep;
                x0 = 0;
            }
            if (x0 >= x1) {
                return;
            }
        }

        offset += x0;
        length = x1 - x0;

        if (Draw3D::alpha == 0) {
            do {
                //int32_t idx = (color0 >> 8) & 0xFFFF;
                //dst[offset++] = palette[idx];
                dst[offset++] = 0xFF000000 | palette[color0 >> 8];
                color0 += colorStep;
            } while (--length > 0);
            return;
        }

        int32_t alpha = Draw3D::alpha;
        int32_t invAlpha = 256 - Draw3D::alpha;

        do {
            //int32_t idx = (color0 >> 8) & 0xFFFF;
            //rgb = 0xFF000000 | palette[idx];
            rgb = palette[color0 >> 8];
            color0 += colorStep;
            rgb = ((((rgb & 0xff00ff) * invAlpha) >> 8) & 0xff00ff) + ((((rgb & 0xff00) * invAlpha) >> 8) & 0xff00);
            // If you want to fix the lines in transparent models like ghostly or bank booths, change dst[offset++] to
            // dst[offset] and on the next line below put offset++
            dst[offset++] = 0xFF000000 | (rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00));
        } while (--length > 0);
    }

    void Draw3D::DrawTexturedScanline(int32_t* dst, std::vector<int32_t>& texels, int32_t curU, int32_t curV, int32_t offset,
        int32_t xA, int32_t xB, int32_t shadeA, int32_t shadeB, int32_t u, int32_t v, int32_t w, int32_t uStride,
        int32_t vStride, int32_t wStride)
    {
        if (xA >= xB) {
            return;
        }

        // Cache texel pointer for faster access in tight loops
        const int32_t* texelPtr = texels.data();

        int32_t shadeStride;
        int32_t strides;

        if (clipX) {
            shadeStride = (shadeB - shadeA) / (xB - xA); // in this form, it's a 'shadeStep'

            if (xB > Draw2D::boundX) {
                xB = Draw2D::boundX;
            }

            if (xA < 0) {
                shadeA -= xA * shadeStride;
                xA = 0;
            }

            if (xA >= xB) {
                return;
            }

            strides = (xB - xA) >> 3;
            shadeStride <<= 12; // this is what transforms it to a stride. it's a (<<9) + (<<3)
        } else {
            if ((xB - xA) > 7) {
                strides = (xB - xA) >> 3;
                shadeStride = ((shadeB - shadeA) * reciprocal15[strides]) >> 6;
            } else {
                strides = 0;
                shadeStride = 0;
            }
        }

        shadeA <<= 9;
        offset += xA;

        if (lowmem) {
            int32_t nextU = 0;
            int32_t nextV = 0;

            int32_t dx = xA - centerX;
            u += (uStride >> 3) * dx;
            v += (vStride >> 3) * dx;
            w += (wStride >> 3) * dx;

            int32_t curW = w >> 12;

            if (curW != 0) {
                curU = u / curW;
                curV = v / curW;
                if (curU < 0) {
                    curU = 0;
                } else if (curU > 4032) {
                    curU = 4032;
                }
            }

            u += uStride;
            v += vStride;
            w += wStride;
            curW = w >> 12;

            if (curW != 0) {
                nextU = u / curW;
                nextV = v / curW;
                if (nextU < 7) {
                    nextU = 7;
                } else if (nextU > 4032) {
                    nextU = 4032;
                }
            }

            int32_t stepU = (nextU - curU) >> 3;
            int32_t stepV = (nextV - curV) >> 3;

            curU += (shadeA & 0x600000) >> 3; // treat curU like the offset and move to the correct tile in our atlas

            int32_t shadeShift = shadeA >> 23; // always a value 0..3 inclusive

            // If you look @ getTexels() you'll notice that the texels are slightly decimated by performing & 0xF8F8FF
            // on them, this was to clear the lower 3 bits of R,G channels so that u can divide the whole number by 2
            // (or right shift 1) to achieve half values. It's a cheep hax to further darken the color value with 1 op.
            // technically the atlas is 1x4 textures but this trick allows 16 total different shades for each texture.

            if (opaque) {
                while (strides-- > 0) {
                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU += stepU;
                    curV += stepV;

                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU += stepU;
                    curV += stepV;

                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU += stepU;
                    curV += stepV;

                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU += stepU;
                    curV += stepV;

                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU += stepU;
                    curV += stepV;

                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU += stepU;
                    curV += stepV;

                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU += stepU;
                    curV += stepV;

                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU = nextU; // last op just sets to the u/v we expected to end with to avoid accuracy loss
                    curV = nextV;

                    u += uStride;
                    v += vStride;
                    w += wStride;

                    int32_t nextW = w >> 12;

                    if (nextW != 0) { // calculate u/v values for the end of our next stride
                        nextU = u / nextW;
                        nextV = v / nextW;

                        if (nextU < 7) {
                            nextU = 7;
                        } else if (nextU > 4032) {
                            nextU = 4032;
                        }
                    }

                    stepU = (nextU - curU) >> 3; // update our step size
                    stepV = (nextV - curV) >> 3;
                    shadeA += shadeStride;
                    curU += (shadeA & 0x600000) >> 3; // update tile in atlas
                    shadeShift = shadeA >> 23; // update shade
                }

                // handles the remaining pixels if the scanline wasn't divisible by 8
                for (strides = (xB - xA) & 7; strides-- > 0; ) {
                    dst[offset++] = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift;
                    curU += stepU;
                    curV += stepV;
                }
                return;
            }

            while (strides-- > 0) {
                int32_t rgb;
                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;

                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU = nextU;
                curV = nextV;

                u += uStride;
                v += vStride;
                w += wStride;

                int32_t nextW = w >> 12;

                if (nextW != 0) {
                    nextU = u / nextW;
                    nextV = v / nextW;
                    if (nextU < 7) {
                        nextU = 7;
                    } else if (nextU > 4032) {
                        nextU = 4032;
                    }
                }

                stepU = (nextU - curU) >> 3;
                stepV = (nextV - curV) >> 3;
                shadeA += shadeStride;
                curU += (shadeA & 0x600000) >> 3;
                shadeShift = shadeA >> 23;
            }
            for (strides = (xB - xA) & 7; strides-- > 0; ) {
                int32_t rgb;
                if ((rgb = static_cast<uint32_t>(texelPtr[(curV & 0xfc0) + (curU >> 6)]) >> shadeShift) != 0) {
                    dst[offset] = rgb;
                }
                offset++;
                curU += stepU;
                curV += stepV;
            }
            return;
        }

        int32_t nextU = 0;
        int32_t nextV = 0;
        int32_t dx = xA - centerX;
        u += (uStride >> 3) * dx;
        v += (vStride >> 3) * dx;
        w += (wStride >> 3) * dx;
        int32_t curW = w >> 14;
        if (curW != 0) {
            curU = u / curW;
            curV = v / curW;
            if (curU < 0) {
                curU = 0;
            } else if (curU > 16256) {
                curU = 16256;
            }
        }
        u += uStride;
        v += vStride;
        w += wStride;
        curW = w >> 14;
        if (curW != 0) {
            nextU = u / curW;
            nextV = v / curW;
            if (nextU < 7) {
                nextU = 7;
            } else if (nextU > 16256) {
                nextU = 16256;
            }
        }
        int32_t uStep = (nextU - curU) >> 3;
        int32_t vStep = (nextV - curV) >> 3;
        curU += shadeA & 0x600000;

        int32_t shadeShift = shadeA >> 23;
        if (opaque) {
            // Cache constants used in inner loop
            const int32_t vMask = 0x3f80;
            const int32_t alphaPrefix = 0xFF000000;
            const int32_t* texelsPtr = texels.data();

            while (strides-- > 0) {
                // Unrolled loop - 8 pixels at a time
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU += uStep;
                curV += vStep;
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU += uStep;
                curV += vStep;
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU += uStep;
                curV += vStep;
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU += uStep;
                curV += vStep;
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU += uStep;
                curV += vStep;
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU += uStep;
                curV += vStep;
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU += uStep;
                curV += vStep;
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU = nextU;
                curV = nextV;
                u += uStride;
                v += vStride;
                w += wStride;
                int32_t i6 = w >> 14;
                if (i6 != 0) {
                    nextU = u / i6;
                    nextV = v / i6;
                    if (nextU < 7) {
                        nextU = 7;
                    } else if (nextU > 16256) {
                        nextU = 16256;
                    }
                }
                uStep = (nextU - curU) >> 3;
                vStep = (nextV - curV) >> 3;
                shadeA += shadeStride;
                curU += shadeA & 0x600000;
                shadeShift = shadeA >> 23;
            }
            for (strides = (xB - xA) & 7; strides-- > 0; ) {
                dst[offset++] = alphaPrefix | UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift);
                curU += uStep;
                curV += vStep;
            }
            return;
        }
        // Non-opaque path - use cached pointer and constants
        const int32_t vMask = 0x3f80;
        const int32_t alphaPrefix = 0xFF000000;
        const int32_t* texelsPtr = texels.data();

        while (strides-- > 0) {
            int32_t rgb;
            if ((rgb =  UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU += uStep;
            curV += vStep;
            if ((rgb = UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU += uStep;
            curV += vStep;
            if ((rgb = UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU += uStep;
            curV += vStep;
            if ((rgb = UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU += uStep;
            curV += vStep;
            if ((rgb = UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU += uStep;
            curV += vStep;
            if ((rgb = UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU += uStep;
            curV += vStep;
            if ((rgb = UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU += uStep;
            curV += vStep;
            if ((rgb = UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU = nextU;
            curV = nextV;
            u += uStride;
            v += vStride;
            w += wStride;
            int32_t nextW = w >> 14;
            if (nextW != 0) {
                nextU = u / nextW;
                nextV = v / nextW;
                if (nextU < 7) {
                    nextU = 7;
                } else if (nextU > 16256) {
                    nextU = 16256;
                }
            }
            uStep = (nextU - curU) >> 3;
            vStep = (nextV - curV) >> 3;
            shadeA += shadeStride;
            curU += shadeA & 0x600000;
            shadeShift = shadeA >> 23;
        }
        for (int32_t len = (xB - xA) & 7; len-- > 0; ) {
            int32_t rgb;
            if ((rgb = UnsignedShift(texelsPtr[(curV & vMask) + (curU >> 7)], shadeShift)) != 0) {
                dst[offset] = alphaPrefix | rgb;
            }
            offset++;
            curU += uStep;
            curV += vStep;
        }
    }

    void Draw3D::FillTexturedTriangle(int32_t yA, int32_t yB, int32_t yC, int32_t xA, int32_t xB, int32_t xC,
        int32_t shadeA, int32_t shadeB, int32_t shadeC, int32_t txA, int32_t txB, int32_t txC, int32_t tyA, int32_t tyB,
        int32_t tyC, int32_t tzA, int32_t tzB, int32_t tzC, int32_t texture)
    {
        auto& texels = GetTexels(texture);

        opaque = !textureTranslucent[texture];

        int32_t originX = txA;
        int32_t originY = tyA;
        int32_t originZ = tzA;

        int32_t verticalX = originX - txB;
        int32_t verticalY = originY - tyB;
        int32_t verticalZ = originZ - tzB;

        int32_t horizontalX = txC - originX;
        int32_t horizontalY = tyC - originY;
        int32_t horizontalZ = tzC - originZ;

        // ! It's important to know the document referenced above assumes the following coordinate system:
        // +X = Right
        // +Y = Forward
        // +Z = Up

        // RS2 coordinate system is as follows:
        // +X = Right
        // +Y = Down
        // +Z = Forward

        // Which means we must swap Y and Z for our code to coincide.

        // The reason I called horizontals 'stride' and vertical 'step' is because the drawTexturedScanline is unrolled
        // and does 8 pixels per 'stride' as an optimization. If you were to roll the loops in drawTexturedScanline then
        // you can name these StepHorizontal and change the bitshift to << 5 like its vertical sibling.

        // (a << 3) is the same as (a * 8)

        int32_t u = ((horizontalX * originY) - (horizontalY * originX)) << 14;
        int32_t uStrideHorizontal = ((horizontalY * originZ) - (horizontalZ * originY)) << 8;
        int32_t uStepVertical = ((horizontalZ * originX) - (horizontalX * originZ)) << 5;

        int32_t v = ((verticalX * originY) - (verticalY * originX)) << 14;
        int32_t vStrideHorizontal = ((verticalY * originZ) - (verticalZ * originY)) << 8;
        int32_t vStepVertical = ((verticalZ * originX) - (verticalX * originZ)) << 5;

        int32_t w = ((verticalY * horizontalX) - (verticalX * horizontalY)) << 14;
        int32_t wStrideHorizontal = ((verticalZ * horizontalY) - (verticalY * horizontalZ)) << 8;
        int32_t wStepVertical = ((verticalX * horizontalZ) - (verticalZ * horizontalX)) << 5;

        int32_t xStepAB = 0;
        int32_t xStepBC = 0;
        int32_t xStepAC = 0;

        int32_t shadeStepAB = 0;
        int32_t shadeStepBC = 0;
        int32_t shadeStepAC = 0;

        // Simplified/rolled methods here:
        // https://gist.githubusercontent.com/thedaneeffect/557750c7d4b6138c539b5e3e9d934946/raw/c5c119bf3b3a330066f9264668ba706c6f837728/triangular.java

        if (yB != yA) {
            xStepAB = ((xB - xA) << 16) / (yB - yA);
            shadeStepAB = ((shadeB - shadeA) << 16) / (yB - yA);
        }

        if (yC != yB) {
            xStepBC = ((xC - xB) << 16) / (yC - yB);
            shadeStepBC = ((shadeC - shadeB) << 16) / (yC - yB);
        }

        if (yC != yA) {
            xStepAC = ((xA - xC) << 16) / (yA - yC);
            shadeStepAC = ((shadeA - shadeC) << 16) / (yA - yC);
        }

        if ((yA <= yB) && (yA <= yC)) {
            if (yA >= Draw2D::bottom) {
                return;
            }
            if (yB > Draw2D::bottom) {
                yB = Draw2D::bottom;
            }
            if (yC > Draw2D::bottom) {
                yC = Draw2D::bottom;
            }
            if (yB < yC) {
                xC = (xA <<= 16);
                shadeC = (shadeA <<= 16);
                if (yA < 0) {
                    xC -= xStepAC * yA;
                    xA -= xStepAB * yA;
                    shadeC -= shadeStepAC * yA;
                    shadeA -= shadeStepAB * yA;
                    yA = 0;
                }
                xB <<= 16;
                shadeB <<= 16;
                if (yB < 0) {
                    xB -= xStepBC * yB;
                    shadeB -= shadeStepBC * yB;
                    yB = 0;
                }
                int32_t dy = yA - centerY;
                u += uStepVertical * dy;
                v += vStepVertical * dy;
                w += wStepVertical * dy;
                if (((yA != yB) && (xStepAC < xStepAB)) || ((yA == yB) && (xStepAC > xStepBC))) {
                    yC -= yB;
                    yB -= yA;
                    yA = lineOffset[yA];
                    while (--yB >= 0) {
                        DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yA, xC >> 16, xA >> 16, shadeC >> 8, shadeA >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                        xC += xStepAC;
                        xA += xStepAB;
                        shadeC += shadeStepAC;
                        shadeA += shadeStepAB;
                        yA += Draw2D::width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yC >= 0) {
                        DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yA, xC >> 16, xB >> 16, shadeC >> 8, shadeB >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                        xC += xStepAC;
                        xB += xStepBC;
                        shadeC += shadeStepAC;
                        shadeB += shadeStepBC;
                        yA += Draw2D::width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    return;
                }
                yC -= yB;
                yB -= yA;
                yA = lineOffset[yA];
                while (--yB >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yA, xA >> 16, xC >> 16, shadeA >> 8, shadeC >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xC += xStepAC;
                    xA += xStepAB;
                    shadeC += shadeStepAC;
                    shadeA += shadeStepAB;
                    yA += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yC >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yA, xB >> 16, xC >> 16, shadeB >> 8, shadeC >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xC += xStepAC;
                    xB += xStepBC;
                    shadeC += shadeStepAC;
                    shadeB += shadeStepBC;
                    yA += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                return;
            }
            xB = (xA <<= 16);
            shadeB = (shadeA <<= 16);
            if (yA < 0) {
                xB -= xStepAC * yA;
                xA -= xStepAB * yA;
                shadeB -= shadeStepAC * yA;
                shadeA -= shadeStepAB * yA;
                yA = 0;
            }
            xC <<= 16;
            shadeC <<= 16;
            if (yC < 0) {
                xC -= xStepBC * yC;
                shadeC -= shadeStepBC * yC;
                yC = 0;
            }
            int32_t dy = yA - centerY;
            u += uStepVertical * dy;
            v += vStepVertical * dy;
            w += wStepVertical * dy;
            if (((yA != yC) && (xStepAC < xStepAB)) || ((yA == yC) && (xStepBC > xStepAB))) {
                yB -= yC;
                yC -= yA;
                yA = lineOffset[yA];
                while (--yC >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yA, xB >> 16, xA >> 16, shadeB >> 8, shadeA >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xB += xStepAC;
                    xA += xStepAB;
                    shadeB += shadeStepAC;
                    shadeA += shadeStepAB;
                    yA += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yB >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yA, xC >> 16, xA >> 16, shadeC >> 8, shadeA >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xC += xStepBC;
                    xA += xStepAB;
                    shadeC += shadeStepBC;
                    shadeA += shadeStepAB;
                    yA += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                return;
            }
            yB -= yC;
            yC -= yA;
            yA = lineOffset[yA];
            while (--yC >= 0) {
                DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yA, xA >> 16, xB >> 16, shadeA >> 8, shadeB >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                xB += xStepAC;
                xA += xStepAB;
                shadeB += shadeStepAC;
                shadeA += shadeStepAB;
                yA += Draw2D::width;
                u += uStepVertical;
                v += vStepVertical;
                w += wStepVertical;
            }
            while (--yB >= 0) {
                DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yA, xA >> 16, xC >> 16, shadeA >> 8, shadeC >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                xC += xStepBC;
                xA += xStepAB;
                shadeC += shadeStepBC;
                shadeA += shadeStepAB;
                yA += Draw2D::width;
                u += uStepVertical;
                v += vStepVertical;
                w += wStepVertical;
            }
            return;
        }
        if (yB <= yC) {
            if (yB >= Draw2D::bottom) {
                return;
            }
            if (yC > Draw2D::bottom) {
                yC = Draw2D::bottom;
            }
            if (yA > Draw2D::bottom) {
                yA = Draw2D::bottom;
            }
            if (yC < yA) {
                xA = (xB <<= 16);
                shadeA = (shadeB <<= 16);
                if (yB < 0) {
                    xA -= xStepAB * yB;
                    xB -= xStepBC * yB;
                    shadeA -= shadeStepAB * yB;
                    shadeB -= shadeStepBC * yB;
                    yB = 0;
                }
                xC <<= 16;
                shadeC <<= 16;
                if (yC < 0) {
                    xC -= xStepAC * yC;
                    shadeC -= shadeStepAC * yC;
                    yC = 0;
                }
                int32_t dy = yB - centerY;
                u += uStepVertical * dy;
                v += vStepVertical * dy;
                w += wStepVertical * dy;
                if (((yB != yC) && (xStepAB < xStepBC)) || ((yB == yC) && (xStepAB > xStepAC))) {
                    yA -= yC;
                    yC -= yB;
                    yB = lineOffset[yB];
                    while (--yC >= 0) {
                        DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yB, xA >> 16, xB >> 16, shadeA >> 8, shadeB >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                        xA += xStepAB;
                        xB += xStepBC;
                        shadeA += shadeStepAB;
                        shadeB += shadeStepBC;
                        yB += Draw2D::width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    while (--yA >= 0) {
                        DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yB, xA >> 16, xC >> 16, shadeA >> 8, shadeC >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                        xA += xStepAB;
                        xC += xStepAC;
                        shadeA += shadeStepAB;
                        shadeC += shadeStepAC;
                        yB += Draw2D::width;
                        u += uStepVertical;
                        v += vStepVertical;
                        w += wStepVertical;
                    }
                    return;
                }
                yA -= yC;
                yC -= yB;
                yB = lineOffset[yB];
                while (--yC >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yB, xB >> 16, xA >> 16, shadeB >> 8, shadeA >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xA += xStepAB;
                    xB += xStepBC;
                    shadeA += shadeStepAB;
                    shadeB += shadeStepBC;
                    yB += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yA >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yB, xC >> 16, xA >> 16, shadeC >> 8, shadeA >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xA += xStepAB;
                    xC += xStepAC;
                    shadeA += shadeStepAB;
                    shadeC += shadeStepAC;
                    yB += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                return;
            }
            xC = (xB <<= 16);
            shadeC = (shadeB <<= 16);
            if (yB < 0) {
                xC -= xStepAB * yB;
                xB -= xStepBC * yB;
                shadeC -= shadeStepAB * yB;
                shadeB -= shadeStepBC * yB;
                yB = 0;
            }
            xA <<= 16;
            shadeA <<= 16;
            if (yA < 0) {
                xA -= xStepAC * yA;
                shadeA -= shadeStepAC * yA;
                yA = 0;
            }
            int dy = yB - centerY;
            u += uStepVertical * dy;
            v += vStepVertical * dy;
            w += wStepVertical * dy;
            if (xStepAB < xStepBC) {
                yC -= yA;
                yA -= yB;
                yB = lineOffset[yB];
                while (--yA >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yB, xC >> 16, xB >> 16, shadeC >> 8, shadeB >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xC += xStepAB;
                    xB += xStepBC;
                    shadeC += shadeStepAB;
                    shadeB += shadeStepBC;
                    yB += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yC >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yB, xA >> 16, xB >> 16, shadeA >> 8, shadeB >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xA += xStepAC;
                    xB += xStepBC;
                    shadeA += shadeStepAC;
                    shadeB += shadeStepBC;
                    yB += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                return;
            }
            yC -= yA;
            yA -= yB;
            yB = lineOffset[yB];
            while (--yA >= 0) {
                DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yB, xB >> 16, xC >> 16, shadeB >> 8, shadeC >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                xC += xStepAB;
                xB += xStepBC;
                shadeC += shadeStepAB;
                shadeB += shadeStepBC;
                yB += Draw2D::width;
                u += uStepVertical;
                v += vStepVertical;
                w += wStepVertical;
            }
            while (--yC >= 0) {
                DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yB, xB >> 16, xA >> 16, shadeB >> 8, shadeA >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                xA += xStepAC;
                xB += xStepBC;
                shadeA += shadeStepAC;
                shadeB += shadeStepBC;
                yB += Draw2D::width;
                u += uStepVertical;
                v += vStepVertical;
                w += wStepVertical;
            }
            return;
        }
        if (yC >= Draw2D::bottom) {
            return;
        }
        if (yA > Draw2D::bottom) {
            yA = Draw2D::bottom;
        }
        if (yB > Draw2D::bottom) {
            yB = Draw2D::bottom;
        }
        if (yA < yB) {
            xB = (xC <<= 16);
            shadeB = (shadeC <<= 16);
            if (yC < 0) {
                xB -= xStepBC * yC;
                xC -= xStepAC * yC;
                shadeB -= shadeStepBC * yC;
                shadeC -= shadeStepAC * yC;
                yC = 0;
            }
            xA <<= 16;
            shadeA <<= 16;
            if (yA < 0) {
                xA -= xStepAB * yA;
                shadeA -= shadeStepAB * yA;
                yA = 0;
            }
            int dy = yC - centerY;
            u += uStepVertical * dy;
            v += vStepVertical * dy;
            w += wStepVertical * dy;
            if (xStepBC < xStepAC) {
                yB -= yA;
                yA -= yC;
                yC = lineOffset[yC];
                while (--yA >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yC, xB >> 16, xC >> 16, shadeB >> 8, shadeC >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xB += xStepBC;
                    xC += xStepAC;
                    shadeB += shadeStepBC;
                    shadeC += shadeStepAC;
                    yC += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                while (--yB >= 0) {
                    DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yC, xB >> 16, xA >> 16, shadeB >> 8, shadeA >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                    xB += xStepBC;
                    xA += xStepAB;
                    shadeB += shadeStepBC;
                    shadeA += shadeStepAB;
                    yC += Draw2D::width;
                    u += uStepVertical;
                    v += vStepVertical;
                    w += wStepVertical;
                }
                return;
            }
            yB -= yA;
            yA -= yC;
            yC = lineOffset[yC];
            while (--yA >= 0) {
                DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yC, xC >> 16, xB >> 16, shadeC >> 8, shadeB >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                xB += xStepBC;
                xC += xStepAC;
                shadeB += shadeStepBC;
                shadeC += shadeStepAC;
                yC += Draw2D::width;
                u += uStepVertical;
                v += vStepVertical;
                w += wStepVertical;
            }
            while (--yB >= 0) {
                DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yC, xA >> 16, xB >> 16, shadeA >> 8, shadeB >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                xB += xStepBC;
                xA += xStepAB;
                shadeB += shadeStepBC;
                shadeA += shadeStepAB;
                yC += Draw2D::width;
                u += uStepVertical;
                v += vStepVertical;
                w += wStepVertical;
            }
            return;
        }
        xA = (xC <<= 16);
        shadeA = (shadeC <<= 16);
        if (yC < 0) {
            xA -= xStepBC * yC;
            xC -= xStepAC * yC;
            shadeA -= shadeStepBC * yC;
            shadeC -= shadeStepAC * yC;
            yC = 0;
        }
        xB <<= 16;
        shadeB <<= 16;
        if (yB < 0) {
            xB -= xStepAB * yB;
            shadeB -= shadeStepAB * yB;
            yB = 0;
        }
        int l9 = yC - centerY;
        u += uStepVertical * l9;
        v += vStepVertical * l9;
        w += wStepVertical * l9;
        if (xStepBC < xStepAC) {
            yA -= yB;
            yB -= yC;
            yC = lineOffset[yC];
            while (--yB >= 0) {
                DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yC, xA >> 16, xC >> 16, shadeA >> 8, shadeC >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                xA += xStepBC;
                xC += xStepAC;
                shadeA += shadeStepBC;
                shadeC += shadeStepAC;
                yC += Draw2D::width;
                u += uStepVertical;
                v += vStepVertical;
                w += wStepVertical;
            }
            while (--yA >= 0) {
                DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yC, xB >> 16, xC >> 16, shadeB >> 8, shadeC >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
                xB += xStepAB;
                xC += xStepAC;
                shadeB += shadeStepAB;
                shadeC += shadeStepAC;
                yC += Draw2D::width;
                u += uStepVertical;
                v += vStepVertical;
                w += wStepVertical;
            }
            return;
        }
        yA -= yB;
        yB -= yC;
        yC = lineOffset[yC];
        while (--yB >= 0) {
            DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yC, xC >> 16, xA >> 16, shadeC >> 8, shadeA >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
            xA += xStepBC;
            xC += xStepAC;
            shadeA += shadeStepBC;
            shadeC += shadeStepAC;
            yC += Draw2D::width;
            u += uStepVertical;
            v += vStepVertical;
            w += wStepVertical;
        }
        while (--yA >= 0) {
            DrawTexturedScanline(Draw2D::Pixels(), texels, 0, 0, yC, xC >> 16, xB >> 16, shadeC >> 8, shadeB >> 8, u, v, w, uStrideHorizontal, vStrideHorizontal, wStrideHorizontal);
            xB += xStepAB;
            xC += xStepAC;
            shadeB += shadeStepAB;
            shadeC += shadeStepAC;
            yC += Draw2D::width;
            u += uStepVertical;
            v += vStepVertical;
            w += wStepVertical;
        }
    }

    void Draw3D::DrawScanline(int32_t* dst, int32_t offset, int32_t rgb, int32_t x0, int32_t x1)
    {
        if (clipX) {
            if (x1 > Draw2D::boundX) {
                x1 = Draw2D::boundX;
            }
            if (x0 < 0) {
                x0 = 0;
            }
        }

        if (x0 >= x1) {
            return;
        }

        offset += x0;
        int32_t length = (x1 - x0) >> 2;
        const int32_t alphaPrefix = 0xFF000000;

        if (Draw3D::alpha == 0) {
            const int32_t colorWithAlpha = alphaPrefix | rgb;
            while (--length >= 0) {
                dst[offset++] = colorWithAlpha;
                dst[offset++] = colorWithAlpha;
                dst[offset++] = colorWithAlpha;
                dst[offset++] = colorWithAlpha;
            }
            for (length = (x1 - x0) & 3; --length >= 0; ) {
                dst[offset++] = colorWithAlpha;
            }
            return;
        }

        const int32_t alpha = Draw3D::alpha;
        const int32_t invAlpha = 256 - alpha;

        rgb = ((((rgb & 0xff00ff) * invAlpha) >> 8) & 0xff00ff) + ((((rgb & 0xff00) * invAlpha) >> 8) & 0xff00);
        while (--length >= 0) {
            // to fix lines in transparent things: change index operand to 'offset' and add 'offset++' below each line
            dst[offset++] = alphaPrefix | (rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00));
            dst[offset++] = alphaPrefix | (rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00));
            dst[offset++] = alphaPrefix | (rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00));
            dst[offset++] = alphaPrefix | (rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00));
        }

        for (length = (x1 - x0) & 3; --length >= 0; ) {
            dst[offset++] = alphaPrefix | (rgb + ((((dst[offset] & 0xff00ff) * alpha) >> 8) & 0xff00ff) + ((((dst[offset] & 0xff00) * alpha) >> 8) & 0xff00));
        }
    }
}
