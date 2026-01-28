#pragma once
#include "PCH.h"
#include "SeqSkeleton.h"

namespace SDL_Client
{
    class SeqTransform
    {
    public:
        /**
         * This method can read <i>multiple</i> {@link SeqTransform}s which means the input data can be all the transforms
         * related to the loaded skeleton.
         *
         * @param src the animation data.
         * @see #get(int)
         */
        static void Unpack(const std::vector<int8_t>& src);

        /**
         * Initializes the array of instances.
         *
         * @param count the count
         */
        static void Init(int32_t count);

        /**
         * Gets the {@link SeqTransform}
         *
         * @param id the transform id.
         * @return the {@link SeqTransform} or <code>null</code> if it does not exist.
         */
        static std::shared_ptr<SeqTransform> Get(int32_t id);

        /**
         * Syntax sugar.
         *
         * @param id the id
         * @return <code>id == -1</code>
         */
        static bool IsNull(int32_t id);

        /**
         * Nullifies the array of instances.
         */
        static void Unload();
    public:
        static std::vector<SeqTransform> instances;

        /**
         * The skeleton associated to this transform.
         */
        SeqSkeleton skeleton;

        /**
         * The delay in <code>ticks</code>.
         */
        int32_t delay = 0;

        /**
         * The number of operations this transform performs.
         */
        int32_t length = 0;

        /**
         * The list of bases this transform uses.
         */
        std::vector<int32_t> bases;

        /**
         * This transforms parameters.
         */
        std::vector<int32_t> x, y, z;
    };
}