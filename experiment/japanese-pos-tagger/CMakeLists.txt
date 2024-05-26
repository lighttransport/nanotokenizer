cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME train)
set(TAGGER_NAME tagger)
project(${PROJECT_NAME} C CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../../cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../../cmake/sanitizers)

find_package(Threads REQUIRED)

# look into <thisrepo>/cmake/sanitizers
find_package(Sanitizers) # Supply -DSANITIZE_ADDRESS

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# common source files
set(TRAIN_SOURCES
  ../common/zstd.c
  ../common/safetensors.cc
  train.cc
  )

set(TAGGER_SOURCES
  ../common/zstd.c
  ../common/safetensors.cc
  tagger.cc
  )

add_executable(${PROJECT_NAME} ${TRAIN_SOURCES} ${TRAIN_DEP_SOURCES})
add_sanitizers(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PUBLIC ${TRAIN_DEP_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})
target_include_directories(${PROJECT_NAME} PRIVATE "../common")

add_executable(${TAGGER_NAME} ${TAGGER_SOURCES} ${TAGGER_DEP_SOURCES})
add_sanitizers(${TAGGER_NAME})
target_link_libraries(${TAGGER_NAME} PUBLIC ${TAGGER_DEP_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})
target_include_directories(${TAGGER_NAME} PRIVATE "../common")
