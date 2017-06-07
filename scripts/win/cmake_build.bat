@echo off

rem Make the path of the current batch file, the current directory
cd %~dp0
cd ../..

if exist build (
    pushd build
    cmake --build . --clean-first --target all_build --config Debug
    cmake --build . --clean-first --target all_build --config Release
    popd
)
