cmake_minimum_required(VERSION 3.16)
project(cfr-edge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Compiler flags ----
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /arch:AVX2 /DNDEBUG /EHsc /GL")
    set(CMAKE_CXX_FLAGS_DEBUG   "/Zi /Od /EHsc")
else()
    # -march=native enables AVX2 on machines that support it.
    # -flto enables link-time optimisation for ~10% additional speedup.
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -flto")
    set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -march=native")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)

# ---- nlohmann/json (header-only, fetched automatically) ----
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_DIR ${PROJECT_SOURCE_DIR}/include/nlohmann
)
# Only download if the header doesn't already exist
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/include/nlohmann/json.hpp")
    FetchContent_MakeAvailable(nlohmann_json)
endif()
# Add include path so #include <nlohmann/json.hpp> resolves
# (the file is downloaded into include/nlohmann/json.hpp above)

# ---- Sources ----
set(SOURCES
    src/kuhn.cpp
    src/leduc.cpp
    src/cfr.cpp
    src/best_response.cpp
    src/abstraction.cpp
    src/hand_eval.cpp
    src/holdem.cpp
    src/main.cpp
)

add_executable(cfr_solver ${SOURCES})

# ---- OpenMP (optional, for future parallel traversal) ----
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(cfr_solver PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(cfr_solver PRIVATE HAS_OPENMP=1)
    message(STATUS "OpenMP found — parallel deal traversal enabled")
endif()

# ---- JSON output tool (standalone, same sources minus main) ----
# Can be used headlessly: ./json_exporter --out path/to/public/strategies/
set(JSON_SOURCES
    src/kuhn.cpp
    src/leduc.cpp
    src/cfr.cpp
    src/best_response.cpp
    src/abstraction.cpp
    src/hand_eval.cpp
    src/holdem.cpp
    src/json_exporter.cpp
)
if(EXISTS "${PROJECT_SOURCE_DIR}/src/json_exporter.cpp")
    add_executable(json_exporter ${JSON_SOURCES})
    if(OpenMP_CXX_FOUND)
        target_link_libraries(json_exporter PUBLIC OpenMP::OpenMP_CXX)
        target_compile_definitions(json_exporter PRIVATE HAS_OPENMP=1)
    endif()
endif()
