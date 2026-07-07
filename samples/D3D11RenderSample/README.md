# VarjoD3D11RenderSample

Minimal D3D11 rendering sample for VarjoToolkit.

This sample verifies the rendering path through:

```txt
VarjoSession
VarjoFrameInfo
VarjoSwapChain
VarjoMultiProjLayer
VarjoLayerFrame
```

It creates a D3D11 device on the adapter matching the Varjo runtime LUID, creates a Varjo D3D11 swap chain, renders a clear color and a simple triangle into each texture-array slice, releases the acquired swap chain image, and submits a multi-projection layer.

## Build

Configure with samples enabled:

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON

cmake --build out\build\default --config Release
```

## Run

```bat
out\build\default\samples\D3D11RenderSample\Release\VarjoD3D11RenderSample.exe --frames 300
```

Use `--frames 0` to render until Ctrl+C.

```bat
out\build\default\samples\D3D11RenderSample\Release\VarjoD3D11RenderSample.exe --frames 0
```

This sample requires Varjo Base and a connected Varjo HMD.
