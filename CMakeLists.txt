cmake_minimum_required(VERSION 3.18)
project(ScreenCapture VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── FFGL SDK path ─────────────────────────────────────────────────────────
# Clone https://github.com/resolume/ffgl and set FFGL_ROOT to its root.
# Example: cmake -DFFGL_ROOT=/path/to/ffgl ..
set(FFGL_ROOT "" CACHE PATH "Root of the FFGL SDK repository")

if(NOT FFGL_ROOT)
    message(FATAL_ERROR
        "Set FFGL_ROOT to the FFGL SDK root directory.\n"
        "  git clone https://github.com/resolume/ffgl\n"
        "  cmake -DFFGL_ROOT=/path/to/ffgl ..")
endif()

# ─── FFGL sources ──────────────────────────────────────────────────────────
set(FFGL_SRC_DIR "${FFGL_ROOT}/source/lib/ffgl")
set(FFGL_SOURCES
    ${FFGL_SRC_DIR}/FFGL.cpp
    ${FFGL_SRC_DIR}/FFGLPluginSDK.cpp
    ${FFGL_SRC_DIR}/FFGLLog.cpp
)
set(FFGL_INCLUDE_DIR ${FFGL_SRC_DIR})

# ─── Plugin target ─────────────────────────────────────────────────────────
add_library(ScreenCapture SHARED
    src/ScreenCapture.cpp
    ${FFGL_SOURCES}
)

target_include_directories(ScreenCapture PRIVATE
    include
    ${FFGL_INCLUDE_DIR}
)

# ─── Platform-specific linkage ─────────────────────────────────────────────
if(WIN32)
    target_link_libraries(ScreenCapture PRIVATE
        opengl32
        gdi32
        user32
    )
    set_target_properties(ScreenCapture PROPERTIES
        OUTPUT_NAME  "ScreenCapture"
        SUFFIX       ".dll"
        PREFIX       ""
    )
    # Export FFGL entry points
    target_sources(ScreenCapture PRIVATE
        ${FFGL_SRC_DIR}/FFGLPluginManager.cpp
    )
    # DEF file so Resolume can find the exported symbols
    set_target_properties(ScreenCapture PROPERTIES
        LINK_FLAGS "/DEF:\"${CMAKE_SOURCE_DIR}/cmake/ScreenCapture.def\""
    )

elseif(APPLE)
    find_library(COREGRAPHICS_LIB CoreGraphics REQUIRED)
    find_library(COREFOUNDATION_LIB CoreFoundation REQUIRED)
    target_link_libraries(ScreenCapture PRIVATE
        "-framework OpenGL"
        ${COREGRAPHICS_LIB}
        ${COREFOUNDATION_LIB}
    )
    set_target_properties(ScreenCapture PROPERTIES
        OUTPUT_NAME  "ScreenCapture"
        SUFFIX       ".dylib"
        PREFIX       ""
        MACOSX_BUNDLE FALSE
    )
endif()

# ─── Install ───────────────────────────────────────────────────────────────
# Windows: copy to Resolume plugin folder
if(WIN32)
    set(RESOLUME_PLUGIN_DIR
        "$ENV{PROGRAMDATA}/Resolume Avenue/vfx"
        CACHE PATH "Resolume VFX/FFGL plugin folder")
    install(TARGETS ScreenCapture
            RUNTIME DESTINATION "${RESOLUME_PLUGIN_DIR}")
endif()

message(STATUS "ScreenCapture FFGL plugin configured. Build with: cmake --build . --config Release")
