#include "VarpType.h"

namespace SDL_Client
{
    std::vector<std::shared_ptr<VarpType>> VarpType::instances;

    void VarpType::Unpack(FileArchive& archive)
    {
        Buffer buffer(archive.Read("varp.dat"));
        int32_t count = buffer.ReadU16();

        if (instances.empty()) {
            instances.resize(count);
        }

        for (int32_t i = 0; i < count; i++) {
            if (instances[i] == nullptr) {
                instances[i] = std::make_shared<VarpType>();
            }
            instances[i]->Read(buffer);
        }
        if (buffer.position != buffer.data.size()) {
            LOG_WARN("varptype load mismatch");
        }
    }

    void VarpType::Read(Buffer& in)
    {
        do {
            int32_t code = in.ReadU8();
            if (code == 0) {
                return;
            } else if (code == 1 || code == 2) {
                in.ReadU8();
            } else if (code == 5) {
                type = in.ReadU16();
            } else if (code == 7 || code == 12) {
                in.Read32();
            } else if (code == 10) {
                in.ReadString();
            } else {
                LOG_WARN("Error unrecognised varp config code: %i", code);
            }
        } while (true);
    }
}