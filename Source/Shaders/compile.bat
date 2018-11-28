echo off

set OUT_DIR=%~dp0..\..\Bin\Assets\Shaders\
IF NOT EXIST "%OUT_DIR%" (
mkdir "%OUT_DIR%"
)

for /r %%i in (*.rchit) do glslangValidator -V "%%i" -o "%OUT_DIR%%%~ni%%~xi.spv"
for /r %%i in (*.rahit) do glslangValidator -V "%%i" -o "%OUT_DIR%%%~ni%%~xi.spv"
for /r %%i in (*.rgen) do glslangValidator -V "%%i" -o "%OUT_DIR%%%~ni%%~xi.spv"
for /r %%i in (*.rmiss) do glslangValidator -V "%%i" -o "%OUT_DIR%%%~ni%%~xi.spv"
for /r %%i in (*.glsl) do glslangValidator -V "%%i" -o "%OUT_DIR%%%~ni%%~xi.spv"

pause
