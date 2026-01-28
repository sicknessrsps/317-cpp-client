#pragma once
#include "PCH.h"
#include "FileArchive.h"
#include "Buffer.h"

namespace SDL_Client
{
    class SeqType
    {
    public:
        static void Unpack(FileArchive& archive);
    public:
        int32_t GetFrameDuration(int32_t frame);
        static std::vector<SeqType> instances;
    public:
        inline static int32_t count = 0;

        /**
         * The amount of frames in this {@link SeqType}.
         */
        int32_t frameCount = 0;

        /**
         * The list of {@link SeqTransform} indices indexed by Frame ID.
         *
         * @see SeqTransform
         */
        std::vector<int32_t> transformIDs;

        /**
         * Auxiliary transform indices appear to only be used by a {@link IfType} of type <code>6</code> as seen in {@link Game#drawParentInterface(IfType, int, int, int)}.
         */
        std::vector<int32_t> auxiliaryTransformIDs;

        /**
         * A list of durations indexed by Frame ID.
         */
        std::vector<int32_t> frameDuration;

        /**
         * The number of frames from the end of this {@link SeqType} used for looping.
         */
        int32_t loopFrameCount = -1;

        /**
         * Used to determine which transform bases a primary frame is allowed to use, and secondary not.
         *
         * @see Model#applyTransforms(int, int, int[])
         */
        std::vector<int32_t> mask;

        /**
         * Adds additional space to the render bounds.
         *
         * @see Scene#addTemporary(Entity, int, int, int, int, int, int, boolean, int)
         */
        bool forwardRenderPadding = false;

        /**
         * The priority.
         */
        int32_t priority = 5;

        /**
         * Allows this {@link SeqType} to override the right hand of a {@link PlayerEntity}.
         *
         * @see PlayerEntity#getSequencedModel()
         */
        int32_t rightHandOverride = -1;

        /**
         * Allows this {@link SeqType} to override the left hand of a {@link PlayerEntity}.
         *
         * @see PlayerEntity#getSequencedModel()
         */
        int32_t leftHandOverride = -1;

        /**
         * How many times this seq is allowed to loop before stopping.
         */
        int32_t loopCount = 99;

        /**
         * If 0, causes faster movement, allows looking at target
         * If 1, pause while moving
         * If 2, does not look at target, continues playing during movement
         *
         * @see Game#updateMovement(PathingEntity)
         */
        int32_t moveStyle = -1;

        /**
         * If 0, allows looking at a target
         * If 1, stops playing on move
         * If 2, does not look at target, continues playing during movement
         *
         * @see Game#updateMovement(PathingEntity)
         */
        int32_t idleStyle = -1;
        /**
         * If 1, restarts the sequence if already playing
         * If 2, does not restart if already playing
         *
         * @see Game#readNPCUpdates()
         */
        int32_t replayStyle = 1;
    private:
        void Load(Buffer& buffer);
    };

}