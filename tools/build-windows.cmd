@echo off
setlocal
where cmake >nul 2>&1 || (echo Install Visual Studio 2022 C++ and CMake.& exit /b 1)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DTAS_REQUIRE_PYTHON_TESTS=ON || exit /b 1
cmake --build build --config Release || exit /b 1
ctest --test-dir build -C Release --output-on-failure --no-tests=error || exit /b 1
echo Build OK. See docs\VALIDACION_WINDOWS.md for the physical audio test.
echo WARNING: Endpoint capture duplicates audio. Do not capture and render to the same device!
