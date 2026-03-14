cmake_minimum_required(VERSION 3.16)
project(cfr-edge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /arch:AVX2 /DNDEBUG /EHsc")
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /EHsc")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES
    src/kuhn.cpp
    src/leduc.cpp
    src/cfr.cpp
    src/best_response.cpp
    src/abstraction.cpp
    src/main.cpp
)

add_executable(cfr_solver ${SOURCES})

find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(cfr_solver PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(cfr_solver PRIVATE HAS_OPENMP=1)
endif()
