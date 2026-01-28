#include "SeqType.h"
#include "SeqTransform.h"

namespace SDL_Client
{

    std::vector<SeqType> SeqType::instances;

    void SeqType::Unpack(FileArchive& archive)
    {
        Buffer buffer(archive.Read("seq.dat"));
        count = buffer.ReadU16();
        instances.resize(count);

        for (int32_t i = 0; i < count; i++) {
            instances[i] = SeqType();
            instances[i].Load(buffer);
        }
    }

    int32_t SeqType::GetFrameDuration(int32_t frame)
    {
        int32_t duration = frameDuration[frame];

        if (duration == 0) {
            auto transform = SeqTransform::Get(transformIDs[frame]);
            if (transform != nullptr) {
                duration = frameDuration[frame] = transform->delay;
            }
        }

        if (duration == 0) {
            duration = 1;
        }

        return duration;
    }

    void SeqType::Load(Buffer& buffer)
    {
        while (true) {
            int32_t code = buffer.ReadU8();

            if (code == 0) {
                break;
            } else if (code == 1) {
                frameCount = buffer.ReadU8();
                transformIDs.resize(frameCount);
                auxiliaryTransformIDs.resize(frameCount);
                frameDuration.resize(frameCount);
                for (int32_t f = 0; f < frameCount; f++) {
                    transformIDs[f] = buffer.ReadU16();
                    auxiliaryTransformIDs[f] = buffer.ReadU16();
                    if (auxiliaryTransformIDs[f] == 65535) {
                        auxiliaryTransformIDs[f] = -1;
                    }
                    frameDuration[f] = buffer.ReadU16();
                }
            } else if (code == 2) {
                loopFrameCount = buffer.ReadU16();
            } else if (code == 3) {
                int32_t amount = buffer.ReadU8();
                mask.resize(amount + 1);
                for (int32_t l = 0; l < amount; l++) {
                    mask[l] = buffer.ReadU8();
                }
                mask[amount] = 9999999;
            } else if (code == 4) {
                forwardRenderPadding = true;
            } else if (code == 5) {
                priority = buffer.ReadU8();
            } else if (code == 6) {
                rightHandOverride = buffer.ReadU16();
            } else if (code == 7) {
                leftHandOverride = buffer.ReadU16();
            } else if (code == 8) {
                loopCount = buffer.ReadU8();
            } else if (code == 9) {
                moveStyle = buffer.ReadU8();
            } else if (code == 10) {
                idleStyle = buffer.ReadU8();
            } else if (code == 11) {
                replayStyle = buffer.ReadU8();
            } else if (code == 12) {
                buffer.Read32();
            } else {
                LOG_ERROR("Error unrecognised seq config code: %i", code);
            }
        }

        if (frameCount == 0) {
            frameCount = 1;
            transformIDs = { -1 };
            auxiliaryTransformIDs = { -1 };
            frameDuration = { -1 };
        }

        if (moveStyle == -1) {
            moveStyle = (!mask.empty()) ? 2 : 0;
        }

        if (idleStyle == -1) {
            idleStyle = (!mask.empty()) ? 2 : 0;
        }
    }
}
