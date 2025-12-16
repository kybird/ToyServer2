@echo off
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=%cd%/vcpkg_installed
cmake --build build
