# HMD Service Performance Tests

VarjoToolkit provides dedicated hardware performance tests for the camera and sensor services. These tests are separate from the short smoke/restart tests and are selected with the CTest label `benchmark`.

## Covered services

- VST distorted-color camera, left and right
- Eye camera, left and right
- Environment cubemap
- IMU / head pose
- Eye tracking

Each test performs a warmup, samples the public FPS or samples-per-second getter once per second, and prints:

- interval sample count and rate
- minimum rate
- average of the per-interval rates
- maximum rate
- average calculated from the cumulative count over the complete measurement interval
- cumulative received, processed, and written counts
- cumulative queue drops and write failures where the service exposes them

A temporary output file is also created and validated for each service.

## Default measurement

- Warmup: 1 second
- Measurement: 10 seconds
- Sampling interval: 1 second

The temporary files are written below:

```text
%TEMP%\VarjoToolkitHmdServiceTests\
```

## Build

```bat
set "VARJO_SDK_ROOT=C:\path\to\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

cmake -S . -B out\build\hmd_tests ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=ON

cmake --build out\build\hmd_tests --config Release --parallel
```

## Run all performance tests

```bat
ctest --test-dir out\build\hmd_tests -C Release -V -L "benchmark"
```

The tests use the CTest resource lock `varjo_hmd`, so they are serialized even when CTest is launched with parallel execution.

## Run one service

```bat
ctest --test-dir out\build\hmd_tests -C Release -V -R VarjoToolkitHmdVstServicePerformanceTest
```

Other test names are:

```text
VarjoToolkitHmdEyeCameraServicePerformanceTest
VarjoToolkitHmdCubemapServicePerformanceTest
VarjoToolkitHmdImuServicePerformanceTest
VarjoToolkitHmdEyeTrackingServicePerformanceTest
```

## Change the duration

```bat
set "VARJOTOOLKIT_HMD_PERFORMANCE_WARMUP_SECONDS=2"
set "VARJOTOOLKIT_HMD_PERFORMANCE_SECONDS=30"

ctest --test-dir out\build\hmd_tests -C Release -V -L "benchmark"
```

Accepted ranges:

- Warmup: 0 to 30 seconds
- Measurement: 3 to 120 seconds

## Optional services

Eye camera, cubemap, and eye tracking may be unavailable because of the connected HMD, runtime support, access settings, or gaze calibration. They return CTest `Skipped` by default.

To require all optional services:

```bat
set "VARJOTOOLKIT_REQUIRE_OPTIONAL_HMD_SERVICES=1"
ctest --test-dir out\build\hmd_tests -C Release -V -L "benchmark"
```

## Pass/fail behavior

The tests fail when:

- the cumulative sample count does not increase during the measurement
- a cumulative count moves backward
- a rate is negative or non-finite
- no positive rate is observed
- a supported camera/raw writer reports a write failure
- received, processed, dropped, and written counters are inconsistent
- an expected output file is missing or empty

Queue drops are reported but do not by themselves fail a test.
