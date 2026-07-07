# VarjoToolkit

VarjoToolkit は、Varjo Native SDK を C++17 から扱いやすくするための薄い wrapper / service library です。

このライブラリの役割は、Varjo Native SDK の session、frame、data stream、MR camera property、chroma key、world / marker tracking、occlusion mesh、swapchain / layer submission への入口を整理することです。画像処理、カメラ SDK 抽象、ML 推論、アプリケーション固有の rendering backend はライブラリ本体に含めません。

## 方針

詳しい設計方針は [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) を参照してください。

要点:

- VarjoToolkit 本体は Varjo Native SDK の薄い C++ wrapper として保つ。
- core library の依存は C++ 標準ライブラリ、Varjo Native SDK、必要最小限の Windows / DirectX SDK 型に限定する。
- OpenCV、camera SDK wrapper、ML framework、UI framework、D3DHelper / D3D11Helper / D3D12Helper には依存しない。
- DirectX 自体は Varjo Native SDK との境界として許可する。`ID3D11Device*` や `ID3D12CommandQueue*` は caller から受け取る。
- D3DHelper などの helper library は、必要であれば application / sample 側で使う。VarjoToolkit 本体には入れない。
- JSON parser などは sample-local なら許容するが、core dependency にはしない。
- Boost は標準ライブラリの亜種として許容可能だが、v0.1.x 現在は依存していない。

## 主な機能

### Core

- `VarjoSession`
- `VarjoFrameInfo`
- `VarjoScopedLock`
- `VarjoEventQueue`

### DataStream

- `VarjoDataStream`
- `VarjoDataStreamBufferLock`
- `VarjoDataStreamFrameQueue`

### MR

- `VarjoCameraProperties`
- `VarjoChromaKey`

### World / Marker

- `VarjoWorld`
- `VarjoMarkerTracker`

### Rendering boundary

- `VarjoOcclusionMesh`
- `VarjoSwapChain`
- `VarjoMultiProjLayer`
- `VarjoLayerFrame`

### Services

- `VarjoEyeTrackingService`
- `VarjoIMUService`
- `VarjoVSTService`
- `VarjoEyeCameraService`
- `VarjoEnvironmentCubemapService`

### Utilities

- `VarjoTimestampMapping`
- `VarjoToolkit::Csv`
- `VarjoToolkit/Version.hpp`

## ディレクトリ構成

```txt
VarjoToolkit/
  CMakeLists.txt
  include/VarjoToolkit/
    Core/
    DataStream/
    MR/
    Rendering/
    World/
    Services/
    Utilities/
  src/
  samples/
  tests/
  docs/
```

## 必要環境

- Windows
- Visual Studio 2022 など、C++17 対応 MSVC 環境
- CMake 3.20 以上
- Varjo Base / Varjo Runtime
- Varjo Native SDK
- `ffmpeg.exe`
  - `VarjoVSTService` で MP4 を出力する場合のみ必要

Boost、OpenCV、JSON library、D3DHelper は core library のビルドに不要です。

## ビルド方法

`VARJO_SDK_ROOT` には Varjo Native SDK のルートディレクトリを指定してください。

```bat
set "VARJO_SDK_ROOT=C:\Program Files\Varjo\Varjo Native SDK"

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON

cmake --build out\build\default --config Release
```

NMake でテストだけをビルドする例:

```bat
cmake -S . -B out\build\test ^
  -G "NMake Makefiles" ^
  -DVARJO_INCLUDE_DIR="%VARJO_SDK_ROOT%\include" ^
  -DVARJO_LIBRARY="%VARJO_SDK_ROOT%\lib\VarjoLib.lib" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON

cmake --build out\build\test
ctest --test-dir out\build\test --output-on-failure
```

## テスト

`VARJOTOOLKIT_BUILD_TESTS=ON` で有効化します。

現在のテスト target:

- `VarjoToolkitCoreSmokeTest`
  - Varjo Base と HMD 接続が必要
- `VarjoToolkitCsvUtilityTest`
  - HMD 不要
- `VarjoToolkitDataStreamWrappersTest`
  - HMD 不要
- `VarjoToolkitCameraPropertiesTest`
  - HMD 不要
- `VarjoToolkitScopedLockChromaKeyTest`
  - HMD 不要
- `VarjoToolkitRenderingWorldWrappersTest`
  - HMD 不要
- `VarjoToolkitQueueTimestampUtilityTest`
  - HMD 不要
- `VarjoToolkitCameraChromaDetailedTest`
  - HMD 不要
- `VarjoToolkitRenderingWorldDetailedTest`
  - HMD 不要

## サンプル実行

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out varjo_service_logs --seconds 30
```

VST 動画出力には `ffmpeg.exe` が必要です。`ffmpeg` を使わない場合は `--no-vst` を指定してください。

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out varjo_service_logs --seconds 30 --no-vst
```

## CMake から利用する

```cmake
add_subdirectory(path/to/VarjoToolkit)

target_link_libraries(YourApp
    PRIVATE
        VarjoToolkit::VarjoToolkit
)
```

## 既知の制限

- `VarjoSwapChain` / `VarjoLayerFrame` は低レイヤ wrapper です。D3D11/D3D12 の実描画 sample はまだ含みません。
- `VarjoEventQueue` は実装済みですが、専用 event logger service はまだありません。
- `VarjoMarkerTracker` は実装済みですが、専用 marker tracking CSV service はまだありません。
- `VarjoOcclusionMesh` は実装済みですが、GPU buffer 化や rendering pipeline 統合はまだありません。
- 実機・Varjo Base・ライセンス/機能許可が必要な API は、HMD 不要テストだけでは完全検証できません。
