# VarjoToolkit

VarjoToolkit は、Varjo Native SDK を C++17 から扱いやすくするための薄い wrapper / service library です。

現在のバージョンは **0.4.0** です。

このライブラリは、session、frame、data stream、MR camera property、chroma key、world / marker tracking、video post process shader、occlusion mesh、swapchain / layer submission、およびカメラ／センササービスへの入口を整理します。画像処理、カメラSDK抽象、ML推論、UI、アプリケーション固有のrendering backendはライブラリ本体に含めません。

## 方針

詳しい設計方針は [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) を参照してください。

- Varjo Native SDK の薄いC++ wrapperとして保つ。
- core依存はC++標準ライブラリ、Varjo Native SDK、必要最小限のWindows / DirectX SDK型に限定する。
- OpenCV、camera SDK wrapper、ML framework、UI framework、D3DHelperには依存しない。
- DirectXはVarjo Native SDKとの境界として許可し、deviceやcommand queueはcallerから受け取る。
- Video Post Process ShaderはSDK機能の薄いwrapperとし、HLSL compileやeffect固有処理は持たない。

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
- `VarjoVideoPostProcessShader`
- `VarjoShaderTextureLock`

### World / Marker

- `VarjoWorld`
- `VarjoMarkerTracker`

### Rendering boundary

- `VarjoOcclusionMesh`
- `VarjoSwapChain`
- `VarjoMultiProjLayer`
- `VarjoLayerFrame`

### Services

- `VarjoEventService`
- `VarjoMarkerTrackingService`
- `VarjoEyeTrackingService`
- `VarjoIMUService`
- `VarjoVSTService`
- `VarjoEyeCameraService`
- `VarjoEnvironmentCubemapService`

### Utilities

- `VarjoTimestampMapping`
- `VarjoToolkit::Csv`
- `VarjoToolkit::SampleRateCounter`
- `VarjoToolkit::RunCountingDeque`
- `VarjoToolkit::RunResetSignal`
- `VarjoToolkit/Version.hpp`

## カメラ／センサ性能メトリクス

カメラとセンサのサービスは、現在の実行区間について受信数、処理数、書き込み数、ドロップ数、FPSまたはsamples/sを取得できます。

```cpp
std::cout
    << "VST L/R: "
    << vstService.getLeftFramesPerSecond() << " / "
    << vstService.getRightFramesPerSecond() << " fps\n"
    << "received=" << vstService.receivedFrameCount()
    << " processed=" << vstService.processedFrameCount()
    << " written=" << vstService.writtenFrameCount()
    << " dropped=" << vstService.droppedFrameCount()
    << " writeFailures=" << vstService.writeFailureCount()
    << '\n'
    << "IMU=" << imuService.getSamplesPerSecond() << " samples/s\n"
    << "Gaze=" << eyeTrackingService.getSamplesPerSecond() << " samples/s\n";
```

レートは正常に受信したサンプル数を基準とします。同じサービスインスタンスを再起動すると、実行区間のカウンタとレートは0へリセットされます。

APIの意味と整合条件は [`docs/HMD_SERVICE_METRICS.md`](docs/HMD_SERVICE_METRICS.md) を参照してください。

## ディレクトリ構成

```text
VarjoToolkit/
  CMakeLists.txt
  cmake/
  include/VarjoToolkit/
    Core/
    DataStream/
    Diagnostics/
    MR/
    Rendering/
    Services/
    Utilities/
    World/
  scripts/
  src/
  samples/
  tests/
  docs/
```

## 必要環境

- Windows
- Visual Studio 2022など、C++17対応MSVC環境
- CMake 3.20以上
- Varjo Base / Varjo Runtime
- Varjo Native SDK
- Varjo experimental headers
  - `VarjoVideoPostProcessShader` を有効にする場合に必要
- `ffmpeg.exe`
  - `VarjoVSTService` のMP4出力とVSTサービス試験に必要
- OpenCppCoverage
  - coverage scriptを使う場合のみ必要

Boost、OpenCV、JSON library、D3DHelperはcore libraryのビルドに不要です。

実行時には `VarjoLib.dll` がPATHから見える必要があります。

```bat
set "VARJO_SDK_ROOT=C:\path\to\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

where VarjoLib.dll
```

## ビルド

```bat
set "VARJO_SDK_ROOT=C:\path\to\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON ^
  -DVARJOTOOLKIT_BUILD_TESTS=OFF ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=OFF

cmake --build out\build\default --config Release --parallel
```

standard SDKなどでexperimental headerがない場合は、次を指定してください。

```bat
-DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=OFF
```

## テスト

テストは2種類に分かれています。

- `VARJOTOOLKIT_BUILD_TESTS=ON`
  - HMD不要のユーティリティテスト
- `VARJOTOOLKIT_BUILD_HMD_TESTS=ON`
  - Varjo Baseと接続済みHMDが必要な実機テスト

### 構成とビルド

```bat
set "VARJO_SDK_ROOT=C:\path\to\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

rmdir /s /q out\build\hmd_tests 2>nul

cmake -S . -B out\build\hmd_tests ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=ON ^
  -DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=ON

cmake --build out\build\hmd_tests --config Debug --parallel
cmake --build out\build\hmd_tests --config Release --parallel
```

### HMD不要テスト

```bat
ctest --test-dir out\build\hmd_tests -C Debug --output-on-failure ^
  -R VarjoToolkitSampleRateUtilitiesTest
```

このテストは、チャンネル別受信数、キュー容量超過、サンプルレート計算、再起動時のリセットを確認します。

### Core / Rendering / D3D smoke test

```bat
ctest --test-dir out\build\hmd_tests -C Debug --output-on-failure ^
  -R "VarjoToolkitHmd(Core|Rendering|D3D11|D3D12)"
```

登録される主なテスト:

- `VarjoToolkitHmdCoreSmokeTest`
- `VarjoToolkitHmdRenderingSmokeTest`
- `VarjoToolkitHmdD3D11SwapChainSmokeTest`
- `VarjoToolkitHmdD3D12SwapChainSmokeTest`

### サービス起動・停止・再起動テスト

```bat
set "VARJOTOOLKIT_HMD_RESTART_CYCLES=3"

ctest --test-dir out\build\hmd_tests -C Debug --output-on-failure -L "service"
```

対象:

- VST
- Eye Camera
- Environment Cubemap
- IMU / Head Pose
- Eye Tracking

再起動回数は1～20回の範囲で変更できます。

```bat
set "VARJOTOOLKIT_HMD_RESTART_CYCLES=10"
ctest --test-dir out\build\hmd_tests -C Debug --output-on-failure -L "restart"
```

### 10秒性能計測

性能試験はReleaseで実行してください。

```bat
set "VARJOTOOLKIT_HMD_PERFORMANCE_WARMUP_SECONDS=1"
set "VARJOTOOLKIT_HMD_PERFORMANCE_SECONDS=10"

ctest --test-dir out\build\hmd_tests -C Release -V -L "benchmark"
```

1秒ごとのレートと、min / average / max / cumulative observed average、受信数、処理数、書き込み数、ドロップ数、書き込み失敗数を表示します。

詳細は [`docs/HMD_SERVICE_PERFORMANCE_TESTS.md`](docs/HMD_SERVICE_PERFORMANCE_TESTS.md) を参照してください。

### 任意機能の扱い

Eye Camera、Cubemap、Eye Trackingがハードウェア、権限、校正状態などにより利用できない場合、対応テストは既定で `Skipped` になります。すべて必須として扱う場合は次を設定します。

```bat
set "VARJOTOOLKIT_REQUIRE_OPTIONAL_HMD_SERVICES=1"
```

### 主なCTestラベル

- `unit`
- `hmd`
- `core`
- `rendering`
- `d3d11`
- `d3d12`
- `service`
- `camera`
- `sensor`
- `restart`
- `performance`
- `benchmark`
- `optional`

HMDを使うテストはCTestの `varjo_hmd` resource lockにより直列実行されます。

### 実機確認済み範囲

v0.4.0の開発時に、次のテストが接続済みHMD環境で通過しています。

- Core smoke test
- Rendering smoke test
- D3D11 swapchain smoke test
- D3D12 swapchain smoke test
- VST service smoke / restart test
- Eye Camera service smoke / restart test
- Environment Cubemap service smoke / restart test
- IMU service smoke / restart test
- Eye Tracking service smoke / restart test
- 上記5サービスの専用performance benchmark

具体的な性能値は環境依存であり、リポジトリには固定値として保存していません。

## install / package

VarjoToolkitは `install()` とCMake package exportに対応しています。

```bat
set "VARJO_SDK_ROOT=C:\path\to\varjo-sdk-experimental"

rmdir /s /q out\build\install 2>nul
rmdir /s /q out\install\VarjoToolkit 2>nul

cmake -S . -B out\build\install ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=OFF ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=OFF ^
  -DCMAKE_INSTALL_PREFIX="%CD%\out\install\VarjoToolkit"

cmake --build out\build\install --config Release
cmake --install out\build\install --config Release
```

consumer project:

```cmake
find_package(VarjoToolkit CONFIG REQUIRED)

target_link_libraries(YourApp
    PRIVATE
        VarjoToolkit::VarjoToolkit
)
```

exportされたtargetにはconfigure時に見つけたVarjo SDK include/libのパスが入ります。別PCへ移す場合はSDKパスを揃えるか、consumer側で再configureしてください。

## coverage

OpenCppCoverage scriptはHMD不要テストを対象にします。

```bat
set "VARJO_SDK_ROOT=C:\path\to\varjo-sdk-experimental"
scripts\run_coverage_open_cpp_coverage.bat
```

出力:

```text
out\coverage\html\index.html
out\coverage\coverage.xml
```

## Video Post Process Shader wrapper

`VarjoVideoPostProcessShader` はNative SDK experimental APIの薄いwrapperです。

- HLSL sourceは受け取らない。
- shader compileは行わない。
- compiled shader bytecodeを `const void* + size` として受け取る。
- constant bufferはraw bytesとして渡す。
- `varjo_LockType_VideoPostProcessShader` をRAIIで保持する。
- shader input textureのacquire/releaseは `VarjoShaderTextureLock` がRAIIで扱う。

```cpp
struct MyConstants {
    float radius;
    float strength;
    int mode;
    int padding;
};

VarjoVideoPostProcessShader shader(session);
auto config = VarjoVideoPostProcessShader::makeVideoPostProcessConfig(sizeof(MyConstants));
shader.configureD3D11(device, config, compiledBlobData, compiledBlobSize);

MyConstants constants{0.25f, 0.8f, 1, 0};
shader.submitConstantBuffer(constants);
shader.setEnabled(true);
```

## サンプル

Service logger:

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe ^
  --out varjo_service_logs --seconds 30
```

VSTを使わない場合:

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe ^
  --out varjo_service_logs --seconds 30 --no-vst
```

D3D11 / D3D12 rendering sampleも含まれています。

## `add_subdirectory` から利用する

```cmake
add_subdirectory(path/to/VarjoToolkit)

target_link_libraries(YourApp
    PRIVATE
        VarjoToolkit::VarjoToolkit
)
```

## 関連文書

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/SUPERDEBUG.md`](docs/SUPERDEBUG.md)
- [`docs/HMD_SERVICE_METRICS.md`](docs/HMD_SERVICE_METRICS.md)
- [`docs/HMD_SERVICE_PERFORMANCE_TESTS.md`](docs/HMD_SERVICE_PERFORMANCE_TESTS.md)
- [`CHANGELOG.md`](CHANGELOG.md)

## 既知の制限

- `VarjoVideoPostProcessShader` はexperimental APIの薄いwrapperであり、HLSL compile、具体的なblur / circular dimming / gaze highlight shader、parameter UIは含みません。
- `VarjoEventService` と `VarjoMarkerTrackingService` はCSV logger付きですが、実際に発生する全イベント／マーカー条件を網羅する実機試験は行っていません。
- `VarjoOcclusionMesh` はGPU buffer化やapplication rendering pipeline統合を行いません。
- performance benchmarkは値を報告し、取得停止、非有限値、カウンタ不整合、書き込み失敗などを検出しますが、特定HMDに対する固定FPS閾値は設定していません。
- 実機、Varjo Base、ライセンス、機能許可が必要なAPIは、HMD不要テストだけでは完全検証できません。
