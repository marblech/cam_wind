cmake_minimum_required(VERSION 3.15)
project(cammon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_DIR /home/marblech/boost_182/lib/cmake/Boost-1.82.0)

set(Boost_USE_STATIC_LIBS OFF)    # 若 b2 构建的是共享库；静态则改为 ON
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_DEBUG_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS system filesystem date_time)

# Output directory setup
set(LIB_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/build)

# Sources - use actual files
set(CAMMON_SOURCES
    src/cammon_api.cpp
    src/protocol.cpp
    src/cammon_jni.cpp
    src/cam_controller.cpp
    src/cam_ajf_lib.cpp
    src/multicast_receiver.cpp
    src/load_settings.h
    src/load_settings.cpp
)

set(CAMMON_HEADERS
    src/protocol.h
    src/cammon_api.h
    src/cam_ajf_lib.h
    src/cam_controller.h
    src/multicast_receiver.h
)

# Build as static library to allow linking C++ symbols into tests
# (avoids needing explicit __declspec(dllexport) on classes)
add_library(cammon SHARED ${CAMMON_SOURCES} ${CAMMON_HEADERS}
    src/plog_init.cpp)

# Ensure CAMMON_EXPORTS is defined when building the DLL so symbols marked with
# CAMMON_API expand to __declspec(dllexport)
target_compile_definitions(cammon PRIVATE CAMMON_EXPORTS)

# Prefer MSVC v142 toolset for compatibility with older consumers if building on Windows
if(MSVC AND WIN32)
    if(NOT DEFINED CMAKE_GENERATOR_TOOLSET)
        set(CMAKE_GENERATOR_TOOLSET "v142" CACHE STRING "MSVC toolset" FORCE)
    endif()
endif()

target_include_directories(cammon PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/plog/include
)

# Boost: add include dirs and link Boost components to cammon
if(Boost_FOUND)
    target_include_directories(cammon PUBLIC ${Boost_INCLUDE_DIRS})
    target_link_libraries(cammon Boost::system Boost::filesystem Boost::date_time)
endif()

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
    target_link_libraries(cammon pthread Boost::system Boost::filesystem Boost::date_time)
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
if(WIN32)
    target_link_libraries(cam_mon_crosshair_test PRIVATE cammon)
else()
    target_link_libraries(cam_mon_crosshair_test PRIVATE cammon pthread)
endif()
set_target_properties(cam_mon_crosshair_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
)

# ============================================================================
# Multicast receiver test
# ============================================================================
add_executable(multicast_receiver_test src/test_multicast.cpp)
if(WIN32)
    target_link_libraries(multicast_receiver_test PRIVATE cammon ws2_32)
else()
    target_link_libraries(multicast_receiver_test PRIVATE cammon pthread)
endif()
set_target_properties(multicast_receiver_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
)

# Example helper: build a small example that demonstrates cam_controller usage
add_executable(cam_controller_example examples/cam_controller_example.cpp)
if(WIN32)
    target_link_libraries(cam_controller_example PRIVATE cammon ws2_32)
else()
    target_link_libraries(cam_controller_example PRIVATE cammon pthread)
endif()
set_target_properties(cam_controller_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
)

# ============================================================================
# PTZ control test
# ============================================================================
add_executable(test_ptz_control src/test_ptz_control.cpp)
if(WIN32)
    target_link_libraries(test_ptz_control PRIVATE cammon ws2_32)
else()
    target_link_libraries(test_ptz_control PRIVATE cammon pthread)
endif()
set_target_properties(test_ptz_control PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
)
