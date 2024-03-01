 cmake_minimum_required(VERSION 3.14.0)
project(CDCache VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# # For debug
# set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_CXX_FLAGS_DEBUG "-g")
# set(CMAKE_CXX_FLAGS_RELEASE "-g")

# # add_compile_options(-fsanitize=address)
# # add_link_options(-fsanitize=address)


# For evaluation
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_DEBUG "-O3 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



include_directories(src/include)
include_directories(third)

enable_testing()
#Catch2 Test lib
add_library(Catch2 third/catch2/catch_amalgamated.cpp)
# add_subdirectory(third/hopscotch-map)

#Cpool


#CDCache
file(GLOB_RECURSE SRC_FILES
        src/*.cpp
        src/*/*.cpp
        src/include/*.h
)

add_library(cdcache ${SRC_FILES})

target_link_libraries(
        cdcache
        xxhash
        ssl
        crypto
        profiler
        z
        pthread
)


# Creat Executable file here
function(create_app name files)
    add_executable(
            ${name}
            ${files}
    )

    target_link_libraries(${name} cdcache)
    set_target_properties(${name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY
            "${PROJECT_SOURCE_DIR}/bin"
    )
endfunction(create_app)

create_app(main_cache_app apps/main_cache_app.cpp)

# tools
create_app(data_hash_generator apps/tools/data_hash_generator.cpp)
create_app(trace_generator apps/tools/trace_generator.cpp)
create_app(trace_analyzer apps/tools/trace_analyzer.cpp)
