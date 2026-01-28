/**
 * @file bzip2-cpp-lib.cpp
 * @author Joshua Jerred (https://joshuajer.red)
 * @brief A C++ wrapper for the bzip2 library.
 * @date 2023-07-04
 * @copyright Copyright (c) 2023
 */

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <bzlib.h>

#include <bzip2-cpp-lib.hpp>

namespace bzip2 {

inline constexpr int kBlockSize100k = 2;
inline constexpr int kVerbosity = 0;
inline constexpr int kWorkFactor = 30;
inline constexpr int kSmall = 0;

inline constexpr int kOutputBufferSize = 1024 * 1024;

Result compress(const std::filesystem::path &input,
                const std::filesystem::path &output) {
  if (!std::filesystem::exists(input)) {
    return Result::INPUT_FILE_DOES_NOT_EXIST;
  }

  // open the input and output files
  std::ifstream input_file(input, std::ios::binary);
  std::ofstream output_file(output, std::ios::binary);
  if (!input_file.is_open()) {
    return Result::FAILED_TO_OPEN_INPUT_FILE;
  }
  if (!output_file.is_open()) {
    return Result::FAILED_TO_OPEN_OUTPUT_FILE;
  }

  // input/output buffers
  std::vector<char> input_buffer(std::istreambuf_iterator<char>(input_file),
                                 {});
  std::vector<char> output_buffer(kOutputBufferSize);

  // initialize the bzip2 stream
  bz_stream stream{};
  if (BZ2_bzCompressInit(&stream, kBlockSize100k, kVerbosity, kWorkFactor) !=
      BZ_OK) {
    return Result::FAILED_TO_INITIALIZE_BZIP2_STREAM;
  }

  stream.next_in = input_buffer.data();
  stream.avail_in = input_buffer.size();

  // compress the input
  int ret;
  while (true) {
    stream.next_out = output_buffer.data();
    stream.avail_out = output_buffer.size();

    ret = BZ2_bzCompress(&stream, stream.avail_in == 0 ? BZ_FINISH : BZ_RUN);

    if (ret == BZ_STREAM_END) {
      break;
    } else if (ret != BZ_RUN_OK) {
      return Result::LOOP_FAILED;
    }

    output_file.write(output_buffer.data(),
                      output_buffer.size() - stream.avail_out);
  }

  output_file.write(output_buffer.data(),
                    output_buffer.size() - stream.avail_out);

  ret = BZ2_bzCompressEnd(&stream);
  return ret == BZ_OK ? Result::SUCCESS : Result::FAILED_TO_END;
}

Result decompress(const std::filesystem::path &input,
                  const std::filesystem::path &output) {
  if (!std::filesystem::exists(input)) {
    return Result::INPUT_FILE_DOES_NOT_EXIST;
  }

  // open the input and output files
  std::ifstream input_file(input, std::ios::binary);
  std::ofstream output_file(output, std::ios::binary);
  if (!input_file.is_open()) {
    return Result::FAILED_TO_OPEN_INPUT_FILE;
  }
  if (!output_file.is_open()) {
    return Result::FAILED_TO_OPEN_OUTPUT_FILE;
  }

  // input/output buffers
  std::vector<char> input_buffer(std::istreambuf_iterator<char>(input_file),
                                 {});
  std::vector<char> output_buffer(kOutputBufferSize);

  // initialize the bzip2 stream
  bz_stream stream{};
  if (BZ2_bzDecompressInit(&stream, kVerbosity, kSmall) != BZ_OK) {
    return Result::FAILED_TO_INITIALIZE_BZIP2_STREAM;
  }

  stream.next_in = input_buffer.data();
  stream.avail_in = input_buffer.size();

  // compress the input
  int ret;
  while (true) {
    stream.next_out = output_buffer.data();
    stream.avail_out = output_buffer.size();

    ret = BZ2_bzDecompress(&stream);

    if (ret == BZ_STREAM_END) {
      break;
    } else if (ret != BZ_RUN_OK) {
      return Result::LOOP_FAILED;
    }

    output_file.write(output_buffer.data(),
                      output_buffer.size() - stream.avail_out);
  }

  output_file.write(output_buffer.data(),
                    output_buffer.size() - stream.avail_out);

  ret = BZ2_bzDecompressEnd(&stream);
  return ret == BZ_OK ? Result::SUCCESS : Result::FAILED_TO_END;
}

} // namespace bzip2
