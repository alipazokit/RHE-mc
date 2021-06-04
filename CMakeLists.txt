cmake_minimum_required(VERSION 3.1)
project(EM_C++)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include/)

set(CMAKE_CXX_FLAGS "-O3")

IF (NOT DEFINED SSE_SUPPORT)
    SET(SSE_SUPPORT 0)
ENDIF()
ADD_DEFINITIONS(-DSSE_SUPPORT=${SSE_SUPPORT})

IF (NOT DEFINED DEBUG)
    SET(DEBUG 0)
ENDIF()
ADD_DEFINITIONS(-DDEBUG=${DEBUG})

add_executable(RHEmc src/rhemc_cov_ldsc_her_def.cpp src/genotype.cpp src/storage.cpp)
add_executable(RHEmc_dom src/dom_jack_stream.cpp src/genotype.cpp src/storage.cpp)
