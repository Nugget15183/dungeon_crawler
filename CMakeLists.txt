cmake_minimum_required(VERSION 4.0)
project(dungeoncrawler)

set(CMAKE_CXX_STANDARD 20)

#build glm
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/packages/glm)

#build glad
add_library(glad STATIC
        packages/glad/src/glad.c
)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/packages/glad/include)

#build glfw
#disable glfw docs + examples
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(packages/glfw-3.5.1)

#build executable
add_executable(dungeoncrawler src/main.cpp
        src/engine.cpp
        src/engine.h
        src/renderer.cpp
        src/renderer.h
        src/logger.h
        src/util.h
        src/stb_image.h)

#link libraries to the executable
target_link_libraries(dungeoncrawler PRIVATE
        glfw
        glad
        glm
)

#move the resources directory to where the .exe is built
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res)