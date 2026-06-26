@echo off
set "JAVA_HOME=C:\Program Files\Java\jdk1.8.0_152"
set PATH=%JAVA_HOME%\bin;%PATH%
cd /d J:\cam_jf\cam_mon_cpp || exit /b 1
if not exist build\jni8 mkdir build\jni8
cd build\jni8 || exit /b 1
cmake .. || exit /b 1
cmake --build . --config Debug || exit /b 1
echo BUILD_OK
