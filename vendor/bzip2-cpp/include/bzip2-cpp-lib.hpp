/**
 * @file bzip2-cpp-lib.hpp
 * @author Joshua Jerred (https://joshuajer.red)
 * @brief A C++ wrapper for the bzip2 library.
 * @date 2023-07-04
 * @copyright Copyright (c) 2023
 */

#ifndef BZIP2_CPP_LIB_HPP_
#define BZIP2_CPP_LIB_HPP_

#include <filesystem>

namespace bzip2 {

/**
 * @brief The result of a bzip2 operation.
 */
enum class Result : int {
  SUCCESS = 0,
  INPUT_FILE_DOES_NOT_EXIST = 1,
  FAILED_TO_OPEN_INPUT_FILE = 2,
  FAILED_TO_OPEN_OUTPUT_FILE = 3,
  FAILED_TO_INITIALIZE_BZIP2_STREAM = 4,
  LOOP_FAILED = 5,
  FAILED_TO_END = 6
};

/**
 * @brief Compresses a file using bzip2.
 *
 * @param input - The path to the input file.
 * @param output - The path to the output file.
 * @return Result - Result::SUCCESS if the operation was successful, otherwise
 * the error code.
 */
Result compress(const std::filesystem::path &input,
                const std::filesystem::path &output);

/**
 * @brief Decompress a file using bzip2.
 *
 * @param input - The path to the input file.
 * @param output - The path to the output file.
 * @return Result - Result::SUCCESS if the operation was successful, otherwise
 * the error code.
 */
Result decompress(const std::filesystem::path &input,
                  const std::filesystem::path &output);

} // namespace bzip2

#endif /* BZIP2_CPP_LIB_HPP_ */