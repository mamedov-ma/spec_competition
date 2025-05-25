cmake_minimum_required (VERSION 3.12)
project (spectal_competition)

set (CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb3 -D_GLIBCXX_DEBUG")
# set (CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -fno-exceptions -fno-rtti  -g -fno-omit-frame-pointer")
set (CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -ffast-math -flto -fno-exceptions -fno-rtti -fomit-frame-pointer -DNDEBUG")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -Wall")

add_executable(solution "")

target_link_options(solution PUBLIC -static-libstdc++ -static-libgcc)

target_sources(solution PRIVATE
    main.cpp
    data_processors/DataFeed.cpp
    MdReceiver/MdReceiver.cpp
)

target_include_directories(solution PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/build_obook
    ${CMAKE_CURRENT_SOURCE_DIR}/data_types
    ${CMAKE_CURRENT_SOURCE_DIR}/data_processors
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/MdReceiver
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(solution pthread numa)
