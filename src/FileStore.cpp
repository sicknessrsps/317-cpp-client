#include "FileStore.h"

namespace SDL_Client {

    // Initialize static buffer
    int8_t FileStore::buf[BUF_SIZE];

    FileStore::FileStore(const int32_t maxFileSize, const std::filesystem::path& datPath, const std::filesystem::path& idxPath, const int32_t store)
        : datPath(datPath), idxPath(idxPath), store(store), maxFileSize(maxFileSize) {
    }

    int64_t FileStore::GetFileCount() {
        std::lock_guard lock(file_mutex);

        SDL_IOStream* idx = SDL_IOFromFile(idxPath.string().c_str(), "rb");
        if (!idx) {
            return 0;
        }

        Sint64 length = SDL_GetIOSize(idx);
        SDL_CloseIO(idx);

        if (length < 0) {
            LOG_ERROR("Failed to get index file size: %s", SDL_GetError());
            return 0;
        }

        return length / 6L;
    }

    std::vector<int8_t> FileStore::Read(int32_t file)
    {
        std::lock_guard lock(file_mutex);

        SDL_IOStream* idx = SDL_IOFromFile(idxPath.string().c_str(), "rb");
        SDL_IOStream* dat = SDL_IOFromFile(datPath.string().c_str(), "rb");

        if (!idx || !dat) {
            if (idx) SDL_CloseIO(idx);
            if (dat) SDL_CloseIO(dat);
            return {};
        }

        // Seek to file entry in index
        if (SDL_SeekIO(idx, file * 6L, SDL_IO_SEEK_SET) < 0) {
            LOG_ERROR("Failed to seek in index: %s", SDL_GetError());
            SDL_CloseIO(idx);
            SDL_CloseIO(dat);
            return {};
        }

        if (SDL_ReadIO(idx, buf, 6) != 6) {
            SDL_CloseIO(idx);
            SDL_CloseIO(dat);
            return {};
        }

        SDL_CloseIO(idx);

        int32_t size = ((buf[0] & 0xff) << 16) + ((buf[1] & 0xff) << 8) + (buf[2] & 0xff);
        int32_t sector = ((buf[3] & 0xff) << 16) + ((buf[4] & 0xff) << 8) + (buf[5] & 0xff);

        if (size > maxFileSize) {
            SDL_CloseIO(dat);
            return {};
        }

        Sint64 datLength = SDL_GetIOSize(dat);
        if (datLength < 0) {
            LOG_ERROR("Failed to get data file size: %s", SDL_GetError());
            SDL_CloseIO(dat);
            return {};
        }

        if (sector <= 0 || static_cast<int64_t>(sector) > (datLength / 520L)) {
            SDL_CloseIO(dat);
            return {};
        }

        std::vector<int8_t> data(size);
        int32_t position = 0;

        for (int32_t part = 0; position < size; part++) {
            if (sector == 0) {
                SDL_CloseIO(dat);
                return {};
            }

            if (SDL_SeekIO(dat, sector * 520L, SDL_IO_SEEK_SET) < 0) {
                LOG_ERROR("Failed to seek in data file: %s", SDL_GetError());
                SDL_CloseIO(dat);
                return {};
            }

            int32_t available = size - position;
            if (available > 512) {
                available = 512;
            }

            if (SDL_ReadIO(dat, buf, available + 8) != static_cast<size_t>(available + 8)) {
                SDL_CloseIO(dat);
                return {};
            }

            const int32_t sectorFile = ((buf[0] & 0xff) << 8) + (buf[1] & 0xff);
            const int32_t sectorPart = ((buf[2] & 0xff) << 8) + (buf[3] & 0xff);
            const int32_t nextSector = ((buf[4] & 0xff) << 16) + ((buf[5] & 0xff) << 8) + (buf[6] & 0xff);
            const int32_t sectorStore = buf[7] & 0xff;

            if (sectorFile != file || sectorPart != part || sectorStore != store) {
                SDL_CloseIO(dat);
                return {};
            }

            if (nextSector < 0 || static_cast<int64_t>(nextSector) > (datLength / 520L)) {
                SDL_CloseIO(dat);
                return {};
            }

            for (int32_t i = 0; i < available; i++) {
                data[position++] = buf[i + 8];
            }

            sector = nextSector;
        }

        SDL_CloseIO(dat);
        return data;
    }

    void FileStore::Write(const std::vector<int8_t> &src, const int32_t file, const int32_t size)
    {
        bool written = Write(src, file, size, true);
        if (!written) {
            Write(src, file, size, false);
        }
    }

    bool FileStore::Write(const std::vector<int8_t> &data, const int32_t file, const int32_t size, bool overwrite) {
        std::lock_guard lock(file_mutex);

        SDL_IOStream* idx = SDL_IOFromFile(idxPath.string().c_str(), "r+b");
        SDL_IOStream* dat = SDL_IOFromFile(datPath.string().c_str(), "r+b");

        if (!idx || !dat) {
            if (idx) SDL_CloseIO(idx);
            if (dat) SDL_CloseIO(dat);
            return false;
        }

        int32_t sector;

        if (overwrite) {
            if (SDL_SeekIO(idx, file * 6L, SDL_IO_SEEK_SET) < 0) {
                LOG_ERROR("Failed to seek in index: %s", SDL_GetError());
                SDL_CloseIO(idx);
                SDL_CloseIO(dat);
                return false;
            }

            if (SDL_ReadIO(idx, buf, 6) != 6) {
                SDL_CloseIO(idx);
                SDL_CloseIO(dat);
                return false;
            }

            sector = ((buf[3] & 0xff) << 16) + ((buf[4] & 0xff) << 8) + (buf[5] & 0xff);

            Sint64 datLength = SDL_GetIOSize(dat);
            if (datLength < 0) {
                LOG_ERROR("Failed to get data file size: %s", SDL_GetError());
                SDL_CloseIO(idx);
                SDL_CloseIO(dat);
                return false;
            }

            if (sector <= 0 || static_cast<int64_t>(sector) > (datLength / 520L)) {
                SDL_CloseIO(idx);
                SDL_CloseIO(dat);
                return false;
            }
        } else {
            Sint64 datLength = SDL_GetIOSize(dat);
            if (datLength < 0) {
                LOG_ERROR("Failed to get data file size: %s", SDL_GetError());
                SDL_CloseIO(idx);
                SDL_CloseIO(dat);
                return false;
            }

            sector = static_cast<int32_t>((datLength + 519L) / 520L);

            if (sector == 0) {
                sector = 1;
            }
        }

        buf[0] = static_cast<uint8_t>(size >> 16);
        buf[1] = static_cast<uint8_t>(size >> 8);
        buf[2] = static_cast<uint8_t>(size);
        buf[3] = static_cast<uint8_t>(sector >> 16);
        buf[4] = static_cast<uint8_t>(sector >> 8);
        buf[5] = static_cast<uint8_t>(sector);

        if (SDL_SeekIO(idx, file * 6L, SDL_IO_SEEK_SET) < 0) {
            LOG_ERROR("Failed to seek in index: %s", SDL_GetError());
            SDL_CloseIO(idx);
            SDL_CloseIO(dat);
            return false;
        }

        if (SDL_WriteIO(idx, buf, 6) != 6) {
            LOG_ERROR("Failed to write to index: %s", SDL_GetError());
            SDL_CloseIO(idx);
            SDL_CloseIO(dat);
            return false;
        }

        if (!SDL_FlushIO(idx)) {
            LOG_ERROR("Failed to flush index: %s", SDL_GetError());
        }

        SDL_CloseIO(idx);

        int32_t written = 0;
        for (int32_t part = 0; written < size; part++) {
            int32_t nextSector = 0;

            if (overwrite) {
                if (SDL_SeekIO(dat, sector * 520L, SDL_IO_SEEK_SET) < 0) {
                    LOG_ERROR("Failed to seek in data file: %s", SDL_GetError());
                    SDL_CloseIO(dat);
                    return false;
                }

                if (SDL_ReadIO(dat, buf, 8) == 8) {
                    int32_t sectorFile = ((buf[0] & 0xff) << 8) + (buf[1] & 0xff);
                    int32_t sectorPart = ((buf[2] & 0xff) << 8) + (buf[3] & 0xff);
                    nextSector = ((buf[4] & 0xff) << 16) + ((buf[5] & 0xff) << 8) + (buf[6] & 0xff);
                    int32_t sectorStore = buf[7] & 0xff;

                    if (sectorFile != file || sectorPart != part || sectorStore != store) {
                        SDL_CloseIO(dat);
                        return false;
                    }

                    Sint64 datLength = SDL_GetIOSize(dat);
                    if (datLength < 0) {
                        LOG_ERROR("Failed to get data file size: %s", SDL_GetError());
                        SDL_CloseIO(dat);
                        return false;
                    }

                    if (nextSector < 0 || static_cast<int64_t>(nextSector) > (datLength / 520L)) {
                        SDL_CloseIO(dat);
                        return false;
                    }
                }
            }

            if (nextSector == 0) {
                overwrite = false;

                Sint64 datLength = SDL_GetIOSize(dat);
                if (datLength < 0) {
                    LOG_ERROR("Failed to get data file size: %s", SDL_GetError());
                    SDL_CloseIO(dat);
                    return false;
                }

                nextSector = static_cast<int32_t>((datLength + 519L) / 520L);

                if (nextSector == 0) {
                    nextSector++;
                }

                if (nextSector == sector) {
                    nextSector++;
                }
            }

            if ((size - written) <= 512) {
                nextSector = 0;
            }

            buf[0] = static_cast<uint8_t>(file >> 8);
            buf[1] = static_cast<uint8_t>(file);
            buf[2] = static_cast<uint8_t>(part >> 8);
            buf[3] = static_cast<uint8_t>(part);
            buf[4] = static_cast<uint8_t>(nextSector >> 16);
            buf[5] = static_cast<uint8_t>(nextSector >> 8);
            buf[6] = static_cast<uint8_t>(nextSector);
            buf[7] = static_cast<uint8_t>(store);

            if (SDL_SeekIO(dat, sector * 520L, SDL_IO_SEEK_SET) < 0) {
                LOG_ERROR("Failed to seek in data file: %s", SDL_GetError());
                SDL_CloseIO(dat);
                return false;
            }

            if (SDL_WriteIO(dat, buf, 8) != 8) {
                LOG_ERROR("Failed to write header to data file: %s", SDL_GetError());
                SDL_CloseIO(dat);
                return false;
            }

            int32_t available = size - written;
            if (available > 512) {
                available = 512;
            }

            if (SDL_WriteIO(dat, data.data() + written, available) != static_cast<size_t>(available)) {
                LOG_ERROR("Failed to write data: %s", SDL_GetError());
                SDL_CloseIO(dat);
                return false;
            }

            if (!SDL_FlushIO(dat)) {
                LOG_ERROR("Failed to flush data file: %s", SDL_GetError());
            }

            written += available;
            sector = nextSector;
        }

        SDL_CloseIO(dat);
        return true;
    }

    void FileStore::Unload()
    {
        // Files are opened/closed on-demand, nothing to unload
        datPath.clear();
        idxPath.clear();
    }
}