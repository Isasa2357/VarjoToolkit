# SuperDebug diagnostics

SuperDebug is a compile-time diagnostics mode for investigating runtime failures in VarjoToolkit.

It is intentionally not a separate CMake configuration. Instead, it is enabled by combining:

```txt
Debug configuration
VARJOTOOLKIT_ENABLE_SUPERDEBUG=ON
```

When both are true, the public compile definition `VARJOTOOLKIT_SUPERDEBUG=1` is set. In all other configurations it is `0`.

## Build

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

git fetch origin
git checkout feature/superdebug-console-diagnostics
git pull --ff-only

rmdir /s /q out\build\superdebug 2>nul

cmake -S . -B out\build\superdebug ^
  -G "Visual Studio 18 2026" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=ON ^
  -DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=ON ^
  -DVARJOTOOLKIT_ENABLE_SUPERDEBUG=ON

cmake --build out\build\superdebug --config Debug
```

## Run HMD tests with SuperDebug output

```bat
ctest --test-dir out\build\superdebug -C Debug -L hmd --output-on-failure
```

Useful targeted runs:

```bat
ctest --test-dir out\build\superdebug -C Debug -L rendering --output-on-failure
ctest --test-dir out\build\superdebug -C Debug -L d3d11 --output-on-failure
ctest --test-dir out\build\superdebug -C Debug -L d3d12 --output-on-failure
ctest --test-dir out\build\superdebug -C Debug -L vpp --output-on-failure
ctest --test-dir out\build\superdebug -C Debug -L mr --output-on-failure
ctest --test-dir out\build\superdebug -C Debug -L datastream --output-on-failure
```

## Output

SuperDebug currently writes to the console through `std::clog`.

Example line:

```txt
[VarjoToolkit][SuperDebug][123ms][tid=1234][INFO] VarjoSession.cpp:56 initialize - session initialized session=00000123456789AB
```

Each line includes:

```txt
elapsed milliseconds since first SuperDebug log
thead id
level
source file and line
function
message
```

## Current instrumented areas

```txt
VarjoSession
VarjoFrameInfo
VarjoEventQueue
VarjoDataStream
VarjoWorld
VarjoOcclusionMesh
VarjoSwapChain
VarjoMultiProjLayer
VarjoLayerFrame
VarjoVideoPostProcessShader
VarjoShaderTextureLock
```

The logs focus on:

```txt
session/runtime availability
frame wait/snapshot state
swapchain creation/acquire/release
layer begin/end/submit parameters
video post process lock/configure/enable/submit failures
stream config enumeration/start/stop/callback dispatch
world/marker object queries
occlusion mesh creation/snapshot
```

## Implementation notes

The diagnostics API is header-only:

```txt
include/VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp
```

Use these macros in implementation files:

```cpp
VTK_SD_LOG("message=" << value);
VTK_SD_WARN("message=" << value);
VTK_SD_ERROR("message=" << value);
VTK_SD_TRACE("message=" << value);
VTK_SD_SCOPE("scope name");
```

When `VARJOTOOLKIT_SUPERDEBUG=0`, these macros compile to no-op statements and do not evaluate their message expressions.
