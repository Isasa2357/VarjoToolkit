# VarjoD3D12RenderSample

Minimal D3D12 rendering sample for VarjoToolkit.

This sample creates a D3D12 device and command queue on the adapter matching the Varjo runtime LUID. It then creates a Varjo D3D12 swap chain, renders a clear color and a simple triangle into each texture-array slice, releases the acquired image, and submits a multi-projection layer.

## Build

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON

cmake --build out\build\default --config Release
```

## Run

```bat
out\build\default\samples\D3D12RenderSample\Release\VarjoD3D12RenderSample.exe --frames 300
```

Use `--frames 0` to render until Ctrl+C.

This sample requires Varjo Base and a connected Varjo HMD.
