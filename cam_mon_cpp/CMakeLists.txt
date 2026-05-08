cmake_minimum_required(VERSION 3.15)
project(cammon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directory setup
set(LIB_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/build)

# Sources - use actual files
set(CAMMON_SOURCES
    src/cammon_api.cpp
    src/protocol.cpp
    src/cammon_jni.cpp
    src/cam_controller.cpp
    src/cam_ajf_lib.cpp
)

set(CAMMON_HEADERS
    src/protocol.h
    src/cammon_api.h
    src/cam_ajf_lib.h
    src/cam_controller.h
)

# Build as shared library (DLL on Windows, SO on Linux)
add_library(cammon SHARED ${CAMMON_SOURCES} ${CAMMON_HEADERS})

target_include_directories(cammon PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Platform-specific settings
if(WIN32)
    # Windows: Use Winsock, define exports
    target_compile_definitions(cammon PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        CAMMON_EXPORTS
    )
    # Link Winsock and JNI
    target_link_libraries(cammon ws2_32)
    target_include_directories(cammon PRIVATE
        $ENV{JAVA_HOME}/include
        $ENV{JAVA_HOME}/include/win32
    )
    # Set output name and suffix
    set_target_properties(cammon PROPERTIES
        OUTPUT_NAME cammon
        SUFFIX .dll
        RUNTIME_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    )
else()
    # Linux
    target_link_libraries(cammon pthread)
    target_include_directories(cammon PRIVATE
        $ENV{JAVA_HOME}/include
        $ENV{JAVA_HOME}/include/linux
    )
    set_target_properties(cammon PROPERTIES
        OUTPUT_NAME cammon
        SUFFIX .so
        RUNTIME_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    )
endif()

# Install rules
install(TARGETS cammon
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)


install(FILES ${CAMMON_HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/cammon)

# ============================================================================
# Test executables
# ============================================================================
add_executable(cam_mon_crosshair_test src/test_crosshair.cpp)
# Link with cammon shared library
target_link_libraries(cam_mon_crosshair_test PRIVATE cammon pthread)
set_target_properties(cam_mon_crosshair_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
)
