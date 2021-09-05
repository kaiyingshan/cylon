##
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##

set(ARROW_HOME ${CMAKE_BINARY_DIR}/arrow/install)
set(ARROW_ROOT ${CMAKE_BINARY_DIR}/arrow)

if (CYLON_PARQUET)
    set(PARQUET_ARGS " -DARROW_WITH_BROTLI=ON"
            " -DARROW_WITH_SNAPPY=ON"
            " -DARROW_WITH_ZLIB=ON"
            " -DARROW_PARQUET=ON")
else (CYLON_PARQUET)
    set(PARQUET_ARGS " -DARROW_WITH_BROTLI=OFF"
            " -DARROW_WITH_SNAPPY=OFF"
            " -DARROW_WITH_ZLIB=OFF"
            " -DARROW_PARQUET=OFF")
endif (CYLON_PARQUET)

set(ARROW_CMAKE_ARGS " -DARROW_WITH_LZ4=OFF"
        " -DARROW_WITH_ZSTD=OFF"
        " -DARROW_BUILD_STATIC=ON"
        " -DARROW_BUILD_SHARED=ON"
        " -DARROW_BUILD_TESTS=OFF"
        " -DARROW_TEST_LINKAGE=shared"
        " -DARROW_TEST_MEMCHECK=OFF"
        " -DARROW_BUILD_BENCHMARKS=OFF"
        " -DARROW_IPC=ON"
        " -DARROW_FLIGHT=OFF"
        " -DARROW_COMPUTE=ON"
        " -DARROW_CUDA=OFF"
        " -DARROW_JEMALLOC=OFF"
        " -DARROW_USE_GLOG=OFF"
        " -DARROW_DATASET=ON"
        " -DARROW_BUILD_UTILITIES=OFF"
        " -DARROW_HDFS=OFF"
        " -DCMAKE_VERBOSE_MAKEFILE=ON"
        " -DARROW_TENSORFLOW=OFF"
        " -DARROW_DATASET=OFF"
        " -DARROW_CSV=ON"
        " -DARROW_JSON=ON"
        " -DARROW_BOOST_USE_SHARED=OFF"
        ${PARQUET_ARGS}
        )

if (PYCYLON_BUILD)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    message("Python Executable Path ${Python3_EXECUTABLE}")
    if(WIN32)
        list(APPEND ARROW_CMAKE_ARGS " -DARROW_PYTHON=${PYCYLON_BUILD}"
                " -DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
                " -DBOOST_ROOT=C:/local/boost_1_77_0_b1_rc1")
    else()
        list(APPEND ARROW_CMAKE_ARGS " -DARROW_PYTHON=${PYCYLON_BUILD}"
                " -DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
                )
    endif()
endif (PYCYLON_BUILD)

message("CMake Source Dir :")
message(${CMAKE_SOURCE_DIR})
configure_file("${CMAKE_SOURCE_DIR}/CMake/Templates/Arrow.CMakeLists.txt.cmake"
        "${ARROW_ROOT}/CMakeLists.txt")

file(MAKE_DIRECTORY "${ARROW_ROOT}/build")
file(MAKE_DIRECTORY "${ARROW_ROOT}/install")

execute_process(
        COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE ARROW_CONFIG
        WORKING_DIRECTORY ${ARROW_ROOT})

if (ARROW_CONFIG)
    message(FATAL_ERROR "Configuring Arrow failed: " ${ARROW_CONFIG})
endif (ARROW_CONFIG)

set(PARALLEL_BUILD -j)
if ($ENV{PARALLEL_LEVEL})
    set(NUM_JOBS $ENV{PARALLEL_LEVEL})
    set(PARALLEL_BUILD "${PARALLEL_BUILD}${NUM_JOBS}")
endif ($ENV{PARALLEL_LEVEL})

execute_process(
        COMMAND ${CMAKE_COMMAND} --build ..
        RESULT_VARIABLE ARROW_BUILD
        WORKING_DIRECTORY ${ARROW_ROOT}/build)

if (ARROW_BUILD)
    message(FATAL_ERROR "Building Arrow failed: " ${ARROW_BUILD})
endif (ARROW_BUILD)

message(STATUS "Arrow installed here: " ${ARROW_ROOT}/install)
set(ARROW_LIBRARY_DIR "${ARROW_ROOT}/install/lib")
set(ARROW_INCLUDE_DIR "${ARROW_ROOT}/install/include")

# todo we may be able to remove these!
find_library(ARROW_LIB arrow
        NO_DEFAULT_PATH
        HINTS "${ARROW_LIBRARY_DIR}")
find_library(ARROW_PYTHON arrow_python
        NO_DEFAULT_PATH
        HINTS "${ARROW_LIBRARY_DIR}")

# find packages with the help of arrow Find*.cmake files
find_package(Arrow REQUIRED HINTS "${ARROW_LIBRARY_DIR}/cmake/arrow" CONFIGS FindArrow.cmake)

if (CYLON_PARQUET)
find_package(Parquet REQUIRED HINTS "${ARROW_LIBRARY_DIR}/cmake/arrow" CONFIGS FindParquet.cmake)
endif (CYLON_PARQUET)

if (PYCYLON_BUILD)
    find_package(arrow_python REQUIRED HINTS "${ARROW_LIBRARY_DIR}/cmake/arrow" CONFIGS FindArrowPython.cmake)
endif (PYCYLON_BUILD)

if (ARROW_LIB)
    message(STATUS "Arrow library: " ${ARROW_LIB})
    set(ARROW_FOUND TRUE)
endif (ARROW_LIB)

set(FLATBUFFERS_ROOT "${ARROW_ROOT}/build/flatbuffers_ep-prefix/src/flatbuffers_ep-install")

message(STATUS "FlatBuffers installed here: " ${FLATBUFFERS_ROOT})
set(FLATBUFFERS_INCLUDE_DIR "${FLATBUFFERS_ROOT}/include")
set(FLATBUFFERS_LIBRARY_DIR "${FLATBUFFERS_ROOT}/lib")

# todo we may be able to remove these!
if (CYLON_PARQUET)
    # todo handle windows
    if(APPLE)
      set(PARQUET_LIB ${ARROW_HOME}/lib/libparquet.dylib)
    else()
      set(PARQUET_LIB ${ARROW_HOME}/lib/libparquet.so)
    endif()
endif (CYLON_PARQUET)

add_definitions(-DARROW_METADATA_V4)
