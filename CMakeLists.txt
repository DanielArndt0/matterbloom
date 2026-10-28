cmake_minimum_required(VERSION 3.16)
project(Matterbloom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy compatibility" FORCE)
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:char8_t-")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG        2.5.1
)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(
    ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.88
)
FetchContent_MakeAvailable(ImGui)

set(SFML_DIR ${sfml_BINARY_DIR})
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL "Don't use find_package for SFML" FORCE)

FetchContent_Declare(
    ImGui-SFML
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG        v2.5
)
FetchContent_MakeAvailable(ImGui-SFML)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

set(SOURCES
    src/App.cpp
    src/WorldEngine.cpp
    src/WorldEnvironment.cpp
    src/WorldPhysics.cpp
    src/WorldArt.cpp
    src/MaterialDB.cpp
    src/Chunk.cpp
    src/Biology.cpp
)

add_executable(Matterbloom ${SOURCES})

target_include_directories(Matterbloom PRIVATE src)
target_link_libraries(Matterbloom PRIVATE sfml-graphics sfml-window sfml-system sfml-audio ImGui-SFML::ImGui-SFML nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(Matterbloom PRIVATE comdlg32)
endif()

add_custom_command(TARGET Matterbloom POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/materials.json"
        "$<TARGET_FILE_DIR:Matterbloom>/materials.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/reactions.json"
        "$<TARGET_FILE_DIR:Matterbloom>/reactions.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Matterbloom>/assets"
)
