# VarjoToolkit

VarjoToolkitは、Varjo Native SDKをC++17から扱いやすくするためのwrapper / service libraryです。

現在のバージョンは **0.5.0** です。

## 重要な同期方針

VarjoToolkit 0.5.0以降、ライブラリ内では`varjo_WaitSync`を呼びません。

- `VarjoFrameInfo`から`waitSync()`を削除
- Eye TrackingのFrameInfo取得ワーカーを削除
- IMUサービス内の同期ループを削除
- 同期済み`VarjoFrameInfoSnapshot`を外部からサービスへ投入

同期所有者はレンダラーなど、アプリケーション全体で1つだけにしてください。

```text
renderer / application
    varjo_WaitSync 1回
    VarjoFrameInfoSnapshot作成
        ├─ rendering
        ├─ VarjoEyeTrackingService
        └─ VarjoIMUService
```

詳細は[`docs/EXTERNAL_FRAME_SYNCHRONIZATION.md`](docs/EXTERNAL_FRAME_SYNCHRONIZATION.md)を参照してください。

## 主な機能

### Core

- `VarjoSession`
- `VarjoFrameInfo`
- `VarjoFrameInfoSnapshot`
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

## 外部FrameInfo入力

### 低レベルVarjo Native SDKアプリ

```cpp
VarjoSession session;
VarjoFrameInfo frameInfo(session);

varjo_WaitSync(session.get(), frameInfo.get());
const auto snapshot = frameInfo.snapshot();
```

`snapshot`には次が含まれます。

- view / projection情報
- `displayTime`
- `frameNumber`
- 同じ同期フレームのCenter Pose
- validity flags

### Eye Tracking

```cpp
VarjoEyeTrackingService eyeTracking(
    session.shared(),
    VarjoEyeTrackingProvider::OutputFilterType::NONE,
    VarjoEyeTrackingProvider::OutputFrequency::MAXIMUM,
    "eye_tracking.csv");

eyeTracking.start();
eyeTracking.submitFrameInfo(snapshot);
```

Gaze取得はサービスワーカーで継続します。Gazeの`captureTime`に対応する外部FrameInfoを履歴から選び、投影計算とCSV出力へ利用します。

### IMU / Head Pose

```cpp
VarjoIMUService imu(
    session.shared(),
    L"imu.csv",
    512);

imu.start();
imu.submitFrameInfo(snapshot);
```

IMUサービスはsnapshotのCenter Poseから位置、Euler角、角速度を計算します。投入はキュー操作のみで、計算とCSV書き込みはサービスワーカーが行います。

## VarjoXRとの連携

VarjoXR 0.2.0以降では、レンダリングバックエンドが唯一の同期所有者です。

```cpp
space.update();

const auto snapshot = space.frameInfoSnapshot();
eyeTracking.submitFrameInfo(snapshot);
imu.submitFrameInfo(snapshot);
```

`frameInfoSnapshot()`は保存済みsnapshotを返すgetterであり、追加の`varjo_WaitSync`は発生しません。

## サービスメトリクス

```cpp
std::cout
    << "gaze=" << eyeTracking.receivedSampleCount()
    << " gazeRate=" << eyeTracking.getSamplesPerSecond()
    << " frameInfo=" << eyeTracking.submittedFrameInfoCount()
    << '\n'
    << "imuReceived=" << imu.receivedSampleCount()
    << " imuWritten=" << imu.writtenSampleCount()
    << " imuDropped=" << imu.droppedSampleCount()
    << " imuRate=" << imu.getSamplesPerSecond()
    << '\n';
```

VST、Eye Camera、Environment Cubemapも受信数、処理数、書き込み数、drop数、FPSを取得できます。

## 必要環境

- Windows 10/11 x64
- Visual Studio 2022などC++17対応コンパイラ
- CMake 3.20以上
- Varjo Base / Varjo Runtime
- Varjo Native SDK
- experimental headers（Video Post Process Shader使用時）
- `ffmpeg.exe`（VSTのMP4出力時）

OpenCV、Boost、JSON library、D3DHelperはVarjoToolkit本体には不要です。

## ビルド

```bat
set "VARJO_SDK_ROOT=C:\path\to\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

rmdir /s /q out\build\default 2>nul

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=OFF

cmake --build out\build\default --config Release --parallel
ctest --test-dir out\build\default -C Release --output-on-failure
```

experimental headerがない環境では次を追加します。

```bat
-DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=OFF
```

## HMD実機テスト

```bat
rmdir /s /q out\build\hmd_tests 2>nul

cmake -S . -B out\build\hmd_tests ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON ^
  -DVARJOTOOLKIT_BUILD_HMD_TESTS=ON ^
  -DVARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=ON

cmake --build out\build\hmd_tests --config Release --parallel
ctest --test-dir out\build\hmd_tests -C Release --output-on-failure -L hmd
```

HMDテスト内の`varjo_WaitSync`はテストアプリケーション側の外部ポンプだけが所有します。Eye TrackingとIMUサービスへは同一snapshotを配布します。

## install / package

```bat
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

Consumer側:

```cmake
find_package(VarjoToolkit 0.5.0 EXACT CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE VarjoToolkit::VarjoToolkit)
```

## ディレクトリ構成

```text
include/VarjoToolkit/
src/
samples/
tests/
docs/
```

変更履歴は[`CHANGELOG.md`](CHANGELOG.md)を参照してください。
