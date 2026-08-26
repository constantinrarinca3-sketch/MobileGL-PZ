
## Usage

### Way 1: import targets
Build xxHash targets:

    cd </path/to/xxHash/>
    cmake -S build/cmake -B cmake_build
    cmake --build cmake_build --parallel
    cmake --install cmake_build

Where possible options are:
- `-DXXHASH_BUILD_XXHSUM=<ON|OFF>`: build the command line binary. ON by default
- `-DBUILD_SHARED_LIBS=<ON|OFF>`: build dynamic library. ON by default.
- `-DCMAKE_INSTALL_PREFIX=<path>`: use custom install prefix path.
- `-DDISPATCH=<ON|OFF>`: enable dispatch mode. Default is ON for x64 cpus, OFF otherwise.

Add lines into downstream CMakeLists.txt:

    find_package(xxHash 0.8 CONFIG REQUIRED)
    ...
    target_link_libraries(MyTarget PRIVATE xxHash::xxhash)

### Way 2: Add subdirectory
Add lines into downstream CMakeLists.txt:

    option(BUILD_SHARED_LIBS "Build shared libs" OFF) #optional
    ...
    set(XXHASH_BUILD_ENABLE_INLINE_API OFF) #optional
    set(XXHASH_BUILD_XXHSUM OFF) #optional
    add_subdirectory(</path/to/xxHash/build/cmake/> </path/to/xxHash/build/> EXCLUDE_FROM_ALL)
    ...
    target_link_libraries(MyTarget PRIVATE xxHash::xxhash)

