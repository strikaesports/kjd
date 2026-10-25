cmake_minimum_required(VERSION 3.16)
project(kjd VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings and options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Find packages
find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)

# Check SDL2 via CMake or PkgConfig
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

# Check GLEW via CMake or PkgConfig
find_package(GLEW QUIET)
if(NOT GLEW_FOUND)
    pkg_check_modules(GLEW REQUIRED glew)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/app.cpp
    src/renderer.cpp
    src/scene.cpp
    src/config.cpp
    src/logger.cpp
)

set(HEADERS
    src/app.hpp
    src/renderer.hpp
    src/scene.hpp
    src/config.hpp
    src/logger.hpp
    src/math3d.hpp
)

add_executable(kjd ${SOURCES} ${HEADERS})

target_include_directories(kjd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENGL_INCLUDE_DIRS}
)

if(TARGET SDL2::SDL2)
    target_link_libraries(kjd PRIVATE SDL2::SDL2)
elseif(SDL2_INCLUDE_DIRS)
    target_include_directories(kjd PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(kjd PRIVATE ${SDL2_LIBRARIES})
else()
    target_link_libraries(kjd PRIVATE SDL2)
endif()

if(TARGET GLEW::GLEW)
    target_link_libraries(kjd PRIVATE GLEW::GLEW)
elseif(GLEW_INCLUDE_DIRS)
    target_include_directories(kjd PRIVATE ${GLEW_INCLUDE_DIRS})
    target_link_libraries(kjd PRIVATE ${GLEW_LIBRARIES})
else()
    target_link_libraries(kjd PRIVATE GLEW)
endif()

target_link_libraries(kjd PRIVATE
    OpenGL::GL
    pthread
    m
)

# Copy shaders and default config to build directory for out-of-the-box local execution
add_custom_command(TARGET kjd POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:kjd>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.frag
        $<TARGET_FILE_DIR:kjd>/shaders/
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:kjd>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/default.toml
        $<TARGET_FILE_DIR:kjd>/config/
    COMMENT "Copying assets to build directory"
)

# Installation targets
include(GNUInstallDirs)
install(TARGETS kjd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY shaders/ DESTINATION ${CMAKE_INSTALL_DATADIR}/kjd/shaders)
install(FILES config/default.toml DESTINATION ${CMAKE_INSTALL_DATADIR}/kjd/config)
install(FILES systemd/kjd.service DESTINATION ${CMAKE_INSTALL_DATADIR}/kjd/systemd)
