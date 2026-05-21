@echo off
REM compile_shaders.bat  -  Compile GLSL shaders to SPIR-V
REM Requires glslc from the Vulkan SDK (https://vulkan.lunarg.com/)

set GLSLC=C:\VulkanSDK\1.4.350.0\Bin\glslc.exe
set SRC=shaders
set OUT=shaders\compiled

if not exist %OUT% mkdir %OUT%

echo Compiling vertex shader...
%GLSLC% -fshader-stage=vert %SRC%\world.vert.glsl -o %OUT%\world.vert.spv
if errorlevel 1 ( echo ERROR: vertex shader failed & exit /b 1 )

echo Compiling fragment shader...
%GLSLC% -fshader-stage=frag %SRC%\world.frag.glsl -o %OUT%\world.frag.spv
if errorlevel 1 ( echo ERROR: fragment shader failed & exit /b 1 )

echo Shaders compiled OK to %OUT%\
