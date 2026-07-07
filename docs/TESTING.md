# Testing VarjoToolkit

## HMD-independent tests

Use `VARJOTOOLKIT_BUILD_TESTS=ON` to build tests that do not require Varjo Base or a connected HMD.

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

rmdir /s /q out\build\test 2>nul

cmake -S . -B out\build\test ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=OFF

cmake --build out\build\test --config Debug
ctest --test-dir out\build\test -C Debug --output-on-failure
```

`VarjoToolkitCoreSmokeTest` is not registered in this mode.

## HMD-required tests

Use `VARJOTOOLKIT_BUILD_HMD_TESTS=ON` to add tests that require Varjo Base and a connected Varjo HMD.

```bat
cmake -S . -B out\build\hmd-test ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=ON

cmake --build out\build\hmd-test --config Debug
ctest --test-dir out\build\hmd-test -C Debug -L hmd --output-on-failure
```

## Package consumer test

`VarjoToolkitPackageConsumerTest` is included when `VARJOTOOLKIT_BUILD_TESTS=ON`.

The test performs the following steps through CTest:

```txt
cmake --install <current VarjoToolkit build> --prefix <temporary test install dir>
cmake -S tests/PackageConsumer -B <temporary consumer build dir> -DCMAKE_PREFIX_PATH=<temporary test install dir>
cmake --build <temporary consumer build dir>
```

This verifies that an external project can use:

```cmake
find_package(VarjoToolkit CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE VarjoToolkit::VarjoToolkit)
```
