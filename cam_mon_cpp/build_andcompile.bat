@echo off
cd /d %~dp0
rmdir /s /q build 2>nul
mkdir build
cd build

echo === Configuring CMake with Visual Studio 16 2019 x64 ===
cmake -G "Visual Studio 16 2019" -A x64 ..

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    exit /b 1
)

echo === Building project ===
cmake --build . --config Debug

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo === Build completed successfully ===
echo DLL location: build\Debug\cammon_cpp.dll