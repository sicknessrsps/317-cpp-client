#pragma once
#include "PCH.h"
#include <bzlib.h>

namespace SDL_Client {
     /**
     * BZip2 decompressor class that feeds the header magic to the bzip2 stream.
     */
    class BZip2 {
    private:
        static constexpr int8_t MAGIC[4] = {'B', 'Z', 'h', '1'};

        const int8_t* src;
        int off;
        int len;
        int position;
        int src_position;

        /**
         * Constructs a new helper class to fool that silly compressor.
         *
         * @param src the source.
         * @param off the source offset.
         * @param len the source length.
         */
        BZip2(const int8_t* src, int off, int len)
            : src(src), off(off), len(len), position(0), src_position(0) {
        }

        /**
         * Reads a single byte from the stream.
         * First 4 bytes return the MAGIC header, then reads from source.
         */
        int read() {
            if (position < 4) {
                return MAGIC[position++] & 0xFF;
            }
            if (src_position < len) {
                return src[off + src_position++] & 0xFF;
            }
            return -1; // EOF
        }

    public:
        /**
         * @see decompress(const int8_t*, int, int, int8_t*)
         */
        static std::vector<int8_t> decompress(const int8_t* src, int length) {
            return decompress(src, 0, length, nullptr, 0);
        }

        /**
         * @see decompress(const int8_t*, int, int, int8_t*)
         */
        static std::vector<int8_t> decompress(const int8_t* src, int off, int len) {
            return decompress(src, off, len, nullptr, 0);
        }

        /**
         * Decompresses the source.
         *
         * @param src the source data.
         * @param off the source offset.
         * @param len the source length.
         * @param dst the destination or nullptr to create a new one.
         * @param dst_len the destination length (only used if dst is not nullptr).
         * @return the decompressed data as a vector.
         * @throws std::runtime_error if the stream content is malformed or an I/O error occurs.
         */
        static std::vector<int8_t> decompress(const int8_t* src, int off, int len,
                                              int8_t* dst, int dst_len) {
            BZip2 helper(src, off, len);

            // Prepare input buffer with magic header + data
            std::vector<int8_t> input_buffer;
            input_buffer.reserve(4 + len);

            // Add magic header
            for (int i = 0; i < 4; i++) {
                input_buffer.push_back(MAGIC[i]);
            }

            // Add source data
            for (int i = 0; i < len; i++) {
                input_buffer.push_back(src[off + i]);
            }

            // Initialize bzip2 stream
            bz_stream stream;
            stream.bzalloc = nullptr;
            stream.bzfree = nullptr;
            stream.opaque = nullptr;
            stream.next_in = reinterpret_cast<char*>(input_buffer.data());
            stream.avail_in = input_buffer.size();

            int ret = BZ2_bzDecompressInit(&stream, 0, 0);
            if (ret != BZ_OK) {
                throw std::runtime_error("BZ2_bzDecompressInit failed");
            }

            std::vector<int8_t> result;
            const int BUFFER_SIZE = 4096;
            int8_t buffer[BUFFER_SIZE];

            bool use_provided_dst = (dst != nullptr);
            int total_written = 0;

            try {
                do {
                    stream.next_out = reinterpret_cast<char*>(buffer);
                    stream.avail_out = BUFFER_SIZE;

                    ret = BZ2_bzDecompress(&stream);

                    if (ret != BZ_OK && ret != BZ_STREAM_END) {
                        BZ2_bzDecompressEnd(&stream);
                        throw std::runtime_error("BZ2_bzDecompress failed");
                    }

                    int have = BUFFER_SIZE - stream.avail_out;

                    if (use_provided_dst) {
                        // Copy to provided destination buffer
                        if (total_written + have > dst_len) {
                            BZ2_bzDecompressEnd(&stream);
                            throw std::runtime_error("Destination buffer too small");
                        }
                        std::memcpy(dst + total_written, buffer, have);
                        total_written += have;
                    } else {
                        // Append to result vector
                        result.insert(result.end(), buffer, buffer + have);
                    }

                } while (ret != BZ_STREAM_END);

                BZ2_bzDecompressEnd(&stream);

            } catch (...) {
                BZ2_bzDecompressEnd(&stream);
                throw;
            }

            if (use_provided_dst) {
                // Return empty vector to indicate dst was used
                return std::vector<int8_t>();
            }

            return result;
        }

        /**
         * Convenience method for std::vector input
         */
        static std::vector<int8_t> decompress(const std::vector<int8_t>& src) {
            return decompress(src.data(), 0, src.size(), nullptr, 0);
        }
    };

    // Initialize static member
    constexpr int8_t BZip2::MAGIC[4];
}