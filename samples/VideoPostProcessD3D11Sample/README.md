# VarjoVideoPostProcessD3D11Sample

Minimal D3D11 sample for Varjo Native SDK Video Post Process Shader through VarjoToolkit.

This sample demonstrates:

```txt
VarjoSession
VarjoVideoPostProcessShader
D3D11 compiled compute shader blob
raw constant buffer struct submission
```

The sample does not add HLSL compilation support to VarjoToolkit core. The sample compiles an embedded HLSL compute shader with `D3DCompile`, then passes the resulting bytecode pointer and size to `VarjoVideoPostProcessShader::configureD3D11`.

The shader applies a simple circular highlight effect to VST: the center area remains unchanged and the outside area is dimmed. A small pulse animation updates only the user constant buffer.

## Build

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON ^
  -DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=ON

cmake --build out\build\default --config Release
```

## Run

```bat
out\build\default\samples\VideoPostProcessD3D11Sample\Release\VarjoVideoPostProcessD3D11Sample.exe --seconds 30
```

Useful options:

```bat
out\build\default\samples\VideoPostProcessD3D11Sample\Release\VarjoVideoPostProcessD3D11Sample.exe --seconds 0 --radius 0.25 --dim 0.65
out\build\default\samples\VideoPostProcessD3D11Sample\Release\VarjoVideoPostProcessD3D11Sample.exe --seconds 30 --no-animate
```

This sample requires Varjo Base, a connected Varjo HMD, and VST capability.
