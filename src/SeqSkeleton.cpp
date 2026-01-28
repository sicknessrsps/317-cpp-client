#include "SeqSkeleton.h"

namespace SDL_Client
{
    SeqSkeleton::SeqSkeleton(Buffer& in)
    {
        const int32_t length = in.ReadU8();
        baseTypes.resize(length);
        baseLabels.resize(length);
        for (int32_t group = 0; group < length; group++) {
            baseTypes[group] = in.ReadU8();
        }
        for (int32_t group = 0; group < length; group++) {
            const int32_t count = in.ReadU8();
            baseLabels[group].resize(count);
            for (int32_t child = 0; child < count; child++) {
                baseLabels[group][child] = in.ReadU8();
            }
        }
    }
}
