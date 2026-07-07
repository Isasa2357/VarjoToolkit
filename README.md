# VarjoToolkit

VarjoToolkit は、Varjo Native SDK を C++17 から扱いやすくするための薄い wrapper / service library です。

このライブラリの役割は、Varjo Native SDK の session、frame、data stream、MR camera property、chroma key、world / marker tracking、video post process shader、occlusion mesh、swapchain / layer submission への入口を整理することです。画像処理、カメラ SDK 抽象、ML 推論、アプリケーション固有の rendering backend はライブラリ本体に含めません。

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
- Native SDK の Video Post Process Shader API は VST / video pass-through に対する SDK 機能なので薄い wrapper として扱う。ただし HLSL compile や effect 固有 logic は持たない。

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
- `VarjoToolkit/Version.hpp`

## ディレクトリ構成

```txt
VarjoToolkit/
  CMakeLists.txt
  cmake/
  include/VarjoToolkit/
    Core/
    DataStream/
    MR/
    Rendering/
    World/
    Services/
    Utilities/
  scripts/
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
- Varjo experimental headers
  - `VarjoVideoPostProcessShader` を有効にする場合は `include_experimental` が必要です
- `ffmpeg.exe`
  - `VarjoVSTService` で MP4 を出力する場合のみ必要
- OpenCppCoverage
  - coverage script を使う場合のみ必要

Boost、OpenCV、JSON library、D3DHelper は core library のビルドに不要です。

## ビルド方法

`VARJO_SDK_ROOT` には Varjo Native SDK のルートディレクトリを指定してください。experimental SDK では通常 `include` と `include_experimental` の両方を含むディレクトリです。

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"

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
  -DVARJO_EXPERIMENTAL_INCLUDE_DIR="%VARJO_SDK_ROOT%\include_experimental" ^
  -DVARJO_LIBRARY="%VARJO_SDK_ROOT%\lib\VarjoLib.lib" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON

cmake --build out\build\test
ctest --test-dir out\build\test --output-on-failure
```

standard SDK などで experimental header がない場合は、次を指定してください。

```bat
-DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=OFF
```

## install / package

VarjoToolkit は `install()` と CMake package export に対応しています。

install 例:

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"

rmdir /s /q out\build\install 2>nul
rmdir /s /q out\install\VarjoToolkit 2>nul

cmake -S . -B out\build\install ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=OFF ^
  -DCMAKE_INSTALL_PREFIX="%CD%\out\install\VarjoToolkit"

cmake --build out\build\install --config Release
cmake --install out\build\install --config Release
```

install 後は consumer project 側で `find_package` できます。

```cmake
find_package(VarjoToolkit CONFIG REQUIRED)

target_link_libraries(YourApp
    PRIVATE
        VarjoToolkit::VarjoToolkit
)
```

CMD から consumer を configure する例:

```bat
cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DCMAKE_PREFIX_PATH="C:\path\to\VarjoToolkit\out\install\VarjoToolkit"
```

注意: export された target には configure 時に見つけた Varjo SDK include/lib の絶対パスが入ります。別 PC に移す場合は Varjo SDK のパスを揃えるか、consumer 側で再 configure してください。

## coverage

OpenCppCoverage を使った coverage script を追加しています。HMD がない環境でも実行できるように、`VarjoToolkitCoreSmokeTest` は除外します。

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"

scripts\run_coverage_open_cpp_coverage.bat
```

出力:

```txt
out\coverage\html\index.html
out\coverage\coverage.xml
```

## Video post process shader wrapper

`VarjoVideoPostProcessShader` は Varjo Native SDK experimental の Video Post Process Shader API の薄い wrapper です。

- HLSL source は受け取りません。
- shader compile は行いません。
- compiled shader bytecode を `const void* + size` として受け取ります。
- constant buffer は raw bytes として渡します。
- template helper は任意の trivially copyable struct を byte列として渡すだけです。
- struct の中身は解釈しません。
- `varjo_LockType_VideoPostProcessShader` を RAII で保持します。
- shader input texture の acquire/release は `VarjoShaderTextureLock` が RAII で扱います。

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
- `VarjoToolkitEventMarkerServicesTest`
  - HMD 不要
- `VarjoToolkitVideoPostProcessShaderTest`
  - HMD 不要
  - `VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=ON` の場合のみ有効

## サンプル実行

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out varjo_service_logs --seconds 30
```

VST 動画出力には `ffmpeg.exe` が必要です。`ffmpeg` を使わない場合は `--no-vst` を指定してください。

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out varjo_service_logs --seconds 30 --no-vst
```

event / marker logging を個別に無効化する場合:

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out varjo_service_logs --seconds 30 --no-event --no-marker
```

## CMake から利用する

`add_subdirectory` で使う場合:

```cmake
add_subdirectory(path/to/VarjoToolkit)

target_link_libraries(YourApp
    PRIVATE
        VarjoToolkit::VarjoToolkit
)
```

install 済み package として使う場合:

```cmake
find_package(VarjoToolkit CONFIG REQUIRED)

target_link_libraries(YourApp
    PRIVATE
        VarjoToolkit::VarjoToolkit
)
```

## 既知の制限

- `VarjoVideoPostProcessShader` は Native SDK experimental API の薄い wrapper です。HLSL compile、具体的な blur / circular dimming / gaze highlight shader、parameter UI は含みません。
- `VarjoSwapChain` / `VarjoLayerFrame` は低レイヤ wrapper です。D3D11/D3D12 の実描画 sample はまだ含みません。
- `VarjoEventService` は CSV logger 付きで実装済みですが、実機での event 発生パターン確認は未実施です。
- `VarjoMarkerTrackingService` は CSV logger 付きで実装済みですが、実機での marker 検出確認は未実施です。
- `VarjoOcclusionMesh` は実装済みですが、GPU buffer 化や rendering pipeline 統合はまだありません。
- 実機・Varjo Base・ライセンス/機能許可が必要な API は、HMD 不要テストだけでは完全検証できません。
