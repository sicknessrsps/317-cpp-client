#pragma once
#include "PCH.h"
#include "Buffer.h"

namespace SDL_Client
{
    /**
     * A {@link SeqSkeleton} describes the usage and relationship between groups of vertices.
     *
     * @see Model#applyTransform(int)
     * @see Model#applyTransforms(int, int, int[])
     */
    class SeqSkeleton
    {
    public:
        /**
         * A base can be thought of as the origin of a bone. This operator comes first before any other operations occur.
         */
        static constexpr int32_t OP_BASE = 0;
        static constexpr int32_t OP_TRANSLATE = 1;

        static constexpr int32_t OP_ROTATE = 2;
        static constexpr int32_t OP_SCALE = 3;
        static constexpr int32_t OP_ALPHA = 5;

        /**
         * A base type determines the operation performed on the labels belonging to it.
         *
         * @see #OP_BASE
         * @see #OP_TRANSLATE
         * @see #OP_ROTATE
         * @see #OP_SCALE
         * @see #OP_ALPHA
         */
         std::vector<int32_t> baseTypes;

        /**
        * The labels belonging to the base.
        *
        * @see Model#createLabelReferences()
        */
        std::vector<std::vector<int32_t>> baseLabels;

        /**
         * Constructs a new {@link SeqSkeleton} read from the provided input.
         *
         * @param in the input
         */
        SeqSkeleton(Buffer& in);
        SeqSkeleton() = default;
    };
}
