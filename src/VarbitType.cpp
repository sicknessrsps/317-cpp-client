#include "VarbitType.h"

namespace SDL_Client
{
    std::vector<std::shared_ptr<VarbitType>> VarbitType::instances;

    void VarbitType::Unpack(FileArchive& archive)
    {
        Buffer buffer(archive.Read("varbit.dat"));
        int32_t count = buffer.ReadU16();

        if (instances.empty()) {
            instances.resize(count);
        }

        for (int32_t j = 0; j < count; j++) {
            if (instances[j] == nullptr) {
                instances[j] = std::make_shared<VarbitType>();
            }
            instances[j]->Read(buffer);
        }
        if (buffer.position != buffer.data.size()) {
            LOG_WARN("varbit load mismatch");
        }
    }

    void VarbitType::Read(Buffer& in)
    {
        do {
            int32_t code = in.ReadU8();
            if (code == 0) {
                return;
            } else if (code == 1) {
                varp = in.ReadU16();
                lsb = in.ReadU8();
                msb = in.ReadU8();
            } else if (code == 10) {
                in.ReadString();
            } else if (code == 3 || code == 4) {
                in.Read32();
            } else {
                LOG_WARN("Error unrecognised varbit config code: %i", code);
            }
        } while (true);
    }
}
