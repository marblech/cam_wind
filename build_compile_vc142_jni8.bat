@echo off
rem Call Visual Studio developer command prompt for amd64
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
if errorlevel 1 (
  echo Failed to initialize VS dev environment
  exit /b 1
)

rem Set JAVA_HOME to JDK 1.8 and update PATH for this session
set "JAVA_HOME=C:\Program Files\Java\jdk1.8.0_152"
set PATH=%JAVA_HOME%\bin;%PATH%

cd /d J:\cam_jf\cam_mon_cpp || exit /b 1
if not exist build\jni8 mkdir build\jni8

rem Remove possible old CMake caches in both build and build\jni8
if exist build\CMakeCache.txt del /f /q build\CMakeCache.txt
if exist build\CMakeFiles rd /s /q build\CMakeFiles

cd build\jni8 || exit /b 1

rem Clean previous CMake cache to avoid toolset mismatch
if exist CMakeCache.txt del /f /q CMakeCache.txt
if exist CMakeFiles rd /s /q CMakeFiles

rem Configure using NMake Makefiles (VS dev env determines toolset)
cmake ../.. -G "NMake Makefiles" || (
  echo CMake configuration failed
  exit /b 1
)

rem Build with nmake
nmake || (
  echo Build failed
  exit /b 1
)

echo BUILD_OK
