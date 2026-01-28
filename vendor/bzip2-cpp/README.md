# bzip2-cpp

This is a simple wrapper around the [bzip2 source code](https://sourceware.org/bzip2/).

Test files are from [here](https://corpus.canterbury.ac.nz/descriptions/).

The bzip2 `.c` and `.h` source files have not been modified in any way, they are
in the `bzip2` directory. All other files, other than the license and readme,
have been removed.

# Usage
## Build
```bash
mkdir build && cd build
cmake ..
make

# run tests
./bzip2-cpp-lib-test
```

## CMake
If this repo is a subdirectory of your project:
```cmake
add_subdirectory(bzip2-cpp-lib)

add_executable(my_executable main.cpp)
target_link_libraries(my_executable PRIVATE bzip2-cpp-lib)

```


```cpp
#include <bzip2-cpp-lib.hpp>

int main() {
  std::filesystem::path input_file_path = "input.txt";
  std::filesystem::path compressed_file_path = "output.txt.bz2";
  std::filesystem::path decompressed_file_path = "output.txt";

  bzip2::Result result;
  result = bzip2::compress(input_file_path, compressed_file_path);
  if (result != bzip2::Result::SUCCESS) {
    // error
  }

  result = bzip2::decompress(compressed_file_path, decompressed_file_path);
  if (result != bzip2::Result::SUCCESS) {
    // error
  }

  return 0;
}

```