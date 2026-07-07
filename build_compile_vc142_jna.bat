@echo off
rem ============================================================================
rem Build script for cam_mon_cpp with JNA (Java Native Access)
rem ============================================================================
rem Unlike JNI, JNA does NOT require bridge C++ code (cammon_jni.cpp).
rem JNA directly calls the exported C functions from cammon.dll.
rem ============================================================================

rem Call Visual Studio developer command prompt for amd64
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
if errorlevel 1 (
  echo Failed to initialize VS dev environment
  exit /b 1
)

rem Set JAVA_HOME to JDK 1.8
set "JAVA_HOME=C:\Program Files\Java\jdk1.8.0_152"
set PATH=%JAVA_HOME%\bin;%PATH%

cd /d J:\cam_jf\cam_mon_cpp || exit /b 1
if not exist\build\jna mkdir build\jna

rem Remove possible old CMake caches
if exist build\CMakeCache.txt del /f /q build\CMakeCache.txt
if exist build\CMakeFiles rd /s /q build\CMakeFiles

cd build\jna || exit /b 1

rem Clean previous CMake cache to avoid toolset mismatch
if exist CMakeCache.txt del /f /q CMakeCache.txt
if exist CMakeFiles rd /s /q CMakeFiles

rem Configure using NMake Makefiles (VS dev env determines toolset)
rem Note: JNA does not require JNI bridge code, only cammon.dll is needed
cmake ../.. -G "NMake Makefiles" || (
  echo CMake configuration failed
  exit /b 1
)

rem Build with nmake
nmake || (
  echo Build failed
  exit /b 1
)

echo.
echo ============================================================================
echo BUILD OK - cam_mon_cpp compiled successfully with JNA support
echo Output: J:\cam_jf\cam_mon_cpp\build\jna\cammon.dll
echo ============================================================================
echo.
echo Next steps:
echo 1. Build cam_mon_java: cd J:\cam_jf\cam_mon_java && mvn clean package
echo 2. Run with: java -Djava.library.path=..\cam_mon_cpp\build\jna -jar target\cam_mon_java-1.0-SNAPSHOT.jar
echo.

echo BUILD_OK