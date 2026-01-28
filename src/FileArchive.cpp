#include "FileArchive.h"
#include "Buffer.h"
#include "BZip2.h"

namespace SDL_Client {

    FileArchive::FileArchive(const std::vector<int8_t> &src)
    {
        Load(src);
    }

    void FileArchive::Load(const std::vector<int8_t> &src)
    {
        Buffer buffer(src);

        int32_t unpackedSize = buffer.Read24();
        int32_t packedSize = buffer.Read24();

        if (packedSize != unpackedSize) {
            data.resize(unpackedSize);
            BZip2::decompress(src.data(), 6, packedSize, data.data(), unpackedSize);
            buffer = Buffer(data);
            m_Unpacked = true;
        } else {
            data = src;
            m_Unpacked = false;
        }

        m_FileCount = buffer.ReadU16();
        fileHash.resize(m_FileCount);
        fileSizeInflated.resize(m_FileCount);
        fileSizeDeflated.resize(m_FileCount);
        fileOffset.resize(m_FileCount);

        int32_t offset = buffer.position + (m_FileCount * 10);

        for (int32_t file = 0; file < m_FileCount; ++file) {
            fileHash[file]        = buffer.Read32();
            fileSizeInflated[file] = buffer.Read24();
            fileSizeDeflated[file] = buffer.Read24();
            fileOffset[file]       = offset;
            offset += fileSizeDeflated[file];
        }
    }

    std::vector<int8_t> FileArchive::Read(const std::string &s)
    {
        // Compute hash
        int32_t hash = 0;
        for (char ch : s) {
            hash = (hash * 61 + static_cast<unsigned char>(std::toupper(ch))) - 32;
        }

        // Find file by hash
        for (int32_t file = 0; file < m_FileCount; ++file) {
            if (fileHash[file] != hash) {
                continue;
            }

            std::vector<int8_t> dst(fileSizeInflated[file]);
            std::string buf;
            if (!m_Unpacked) {
                // Decompress into destination
                // The following line crashes the program:
                BZip2::decompress(
                    data.data(),
                    fileOffset[file],
                    fileSizeDeflated[file],
                    dst.data(),
                    fileSizeInflated[file]
                );
            } else {
                // Copy raw inflated data
                std::copy_n(
                    data.begin() + fileOffset[file],
                    fileSizeInflated[file],
                    dst.begin()
                );
            }

            return dst;
        }
        LOG_ERROR("FileArchive: Not found: %s", s.c_str());
        // Not found
        return {};
    }
}
