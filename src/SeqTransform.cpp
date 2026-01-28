#include "SeqTransform.h"
#include "Buffer.h"

namespace SDL_Client
{

    std::vector<SeqTransform> SeqTransform::instances;

    void SeqTransform::Unpack(const std::vector<int8_t>& src)
    {
        Buffer offsets(src);
        offsets.position = src.size() - 8;

        Buffer header(src);
        Buffer tran1(src);
        Buffer tran2(src);
        Buffer del(src);
        Buffer skel(src);

        int32_t offset = 0;
        header.position = offset;
        offset += offsets.ReadU16() + 2;

        tran1.position = offset;
        offset += offsets.ReadU16();

        tran2.position = offset;
        offset += offsets.ReadU16();

        del.position = offset;
        offset += offsets.ReadU16();

        skel.position = offset;

        SeqSkeleton skeleton(skel);

        int32_t frameCount = header.ReadU16();
        std::vector<int32_t> bases(500);
        std::vector<int32_t> x(500);
        std::vector<int32_t> y(500);
        std::vector<int32_t> z(500);

        for (int32_t i = 0; i < frameCount; i++)
        {
            uint16_t index = header.ReadU16();

            SeqTransform transform;

            transform.delay = del.ReadU8();
            transform.skeleton = skeleton;

            int32_t baseCount = header.ReadU8();
            int32_t lastBase = -1;
            int32_t length = 0;

            for (int32_t base = 0; base < baseCount; base++) {
                int32_t flags = tran1.ReadU8();

                if (flags <= 0) {
                    continue;
                }

                if (skeleton.baseTypes[base] != SeqSkeleton::OP_BASE) {
                    // Look for any skipped ORIGIN bases and insert them into this transform.
                    for (int32_t cur = base - 1; cur > lastBase; cur--) {
                        if (skeleton.baseTypes[cur] == SeqSkeleton::OP_BASE) {
                            bases[length] = cur;
                            x[length] = 0;
                            y[length] = 0;
                            z[length] = 0;
                            length++;
                            break;
                        }
                    }
                }

                bases[length] = base;

                int32_t defaultValue = 0;

                if (skeleton.baseTypes[base] == SeqSkeleton::OP_SCALE) {
                    defaultValue = 128;
                }

                if ((flags & 1) != 0) {
                    x[length] = tran2.ReadSmart();
                } else {
                    x[length] = defaultValue;
                }

                if ((flags & 2) != 0) {
                    y[length] = tran2.ReadSmart();
                } else {
                    y[length] = defaultValue;
                }

                if ((flags & 4) != 0) {
                    z[length] = tran2.ReadSmart();
                } else {
                    z[length] = defaultValue;
                }

                lastBase = base;
                length++;
            }

            transform.length = length;
            transform.bases.resize(length);
            transform.x.resize(length);
            transform.y.resize(length);
            transform.z.resize(length);

            for (int32_t j = 0; j < length; j++) {
                transform.bases[j] = bases[j];
                transform.x[j] = x[j];
                transform.y[j] = y[j];
                transform.z[j] = z[j];
            }

            instances[index] = transform;
        }
    }

    void SeqTransform::Init(int32_t count)
    {
        instances.resize(count + 1);
    }

    std::shared_ptr<SeqTransform> SeqTransform::Get(int32_t id)
    {
        if ((instances.empty()) || (id < 0) || (id >= instances.size())) {
            return nullptr;
        } else {
            return std::make_shared<SeqTransform>(instances[id]);
        }
    }

    bool SeqTransform::IsNull(int32_t id)
    {
        return id == -1;
    }

    void SeqTransform::Unload()
    {
        instances.clear();
    }
}
