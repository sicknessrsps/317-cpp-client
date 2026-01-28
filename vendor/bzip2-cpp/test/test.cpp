/**
 * @file test.cpp
 * @author Joshua Jerred (https://joshuajer.red)
 * @brief Simple test file for bzip2-cpp-lib.
 * @date 2023-07-04
 * @copyright Copyright (c) 2023
 */

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <bzip2-cpp-lib.hpp>

/**
 * @brief Get the size of a file in bytes.
 * @param file - path to the file
 * @return int - size of the file in bytes
 */
int getFileSize(const std::filesystem::path &file) {
  return std::filesystem::file_size(file);
}

/**
 * @brief A lazy way to verify that two files are identical.
 * @param f1 - path to the first file
 * @param f2 - path to the second file
 * @return true - if the files are identical
 * @return false - if the files are not identical
 */
bool verifyIdentical(const std::filesystem::path f1,
                     const std::filesystem::path f2) {
  std::ifstream file1(f1, std::ios::binary);
  std::ifstream file2(f2, std::ios::binary);

  while (true) {
    if (file1.get() != file2.get()) {
      return false;
    }
    if (file1.eof() && file2.eof()) {
      return true;
    } else if (file1.eof() || file2.eof()) {
      return false;
    }
  }
}

int main() {
  const std::filesystem::path input_dir = "test_files";
  const std::filesystem::path comp_output_dir = "test_files/compressed_output";
  const std::filesystem::path decomp_output_dir =
      "test_files/decompressed_output";

  const std::array<std::string, 11> test_files = {
      "alice29.txt", "asyoulik.txt", "cp.html",    "fields.c",
      "grammar.lsp", "kennedy.xls",  "lcet10.txt", "plrabn12.txt",
      "ptt5",        "sum",          "xargs.1"};

  // create the output directory (clear it if it already exists)
  if (std::filesystem::exists(comp_output_dir)) {
    std::filesystem::remove_all(comp_output_dir);
  }
  std::filesystem::create_directory(comp_output_dir);
  if (std::filesystem::exists(decomp_output_dir)) {
    std::filesystem::remove_all(decomp_output_dir);
  }
  std::filesystem::create_directory(decomp_output_dir);

  for (const auto &file : test_files) {
    std::filesystem::path input_path = input_dir / file;
    int original_size = getFileSize(input_path);
    int compressed_size = 0;
    int decompressed_size = 0;
    bzip2::Result result = bzip2::compress(input_path, comp_output_dir / file);

    if (result != bzip2::Result::SUCCESS) {
      std::cout << "Failed to compress! Error Code: "
                << static_cast<int>(result) << " on file: " << file
                << std::endl;
      return 1;
    }
    compressed_size = getFileSize(comp_output_dir / file);

    result =
        bzip2::decompress(comp_output_dir / file, decomp_output_dir / file);
    if (result != bzip2::Result::SUCCESS) {
      std::cout << "Failed to decompress! Error Code: "
                << static_cast<int>(result) << " on file: " << file
                << std::endl;
      return 1;
    }
    decompressed_size = getFileSize(decomp_output_dir / file);

    if (!verifyIdentical(input_path, decomp_output_dir / file)) {
      std::cout << "Decompressed file does not match original file: " << file
                << std::endl;
      return 1;
    }

    std::cout << "Compressed " << file << " from " << original_size << " to "
              << compressed_size << " then decompressed to "
              << decompressed_size << std::endl;
  }

  return 0;
}