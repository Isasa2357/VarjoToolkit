# VarjoToolkit

VarjoToolkit は、Varjo Native SDK を使う C++ アプリケーションから、視線、HMD pose / IMU 相当情報、VST カメラ、EyeCamera、EnvironmentCubemap、MR camera property、chroma key、Varjo timestamp 変換を扱いやすくするための C++17 ライブラリです。

このリポジトリは、Varjo の各種データ取得処理を service class と低レイヤ RAII wrapper としてまとめ、サンプルアプリケーションから CSV、動画、raw buffer として記録できるようにすることを目的としています。

## 主な機能

- `VarjoSession`
  - `varjo_SessionInit` / `varjo_SessionShutDown` の RAII wrapper
  - 既存サービスに渡しやすい `std::shared_ptr<varjo_Session>` を保持
- `VarjoFrameInfo`
  - `varjo_CreateFrameInfo` / `varjo_FreeFrameInfo` / `varjo_WaitSync` の RAII wrapper
  - `varjo_Session*`、`std::shared_ptr<varjo_Session>`、`VarjoSession` から構築可能
  - 保存やキュー投入に使える `VarjoFrameInfoSnapshot` を提供
- `VarjoScopedLock`
  - `varjo_Lock` / `varjo_Unlock` の RAII wrapper
  - Camera / ChromaKey / EnvironmentCubemap などの MR lock に利用
- `VarjoDataStream`
  - DataStream config の列挙、条件指定による best config 選択、start/stop、callback dispatch
- `VarjoDataStreamBufferLock`
  - `varjo_LockDataStreamBuffer` / `varjo_UnlockDataStreamBuffer` の RAII wrapper
- `VarjoCameraProperties`
  - Varjo MR camera property の列挙、cache 更新、mode/value 取得、lock 付き set/reset
  - `CameraManager` 相当の処理を UI / logging から分離した wrapper
- `VarjoChromaKey`
  - Varjo MR chroma key の enable/global enable/config get/set/reset
  - HSV chroma key config helper を提供
- `VarjoToolkit::Csv`
  - Varjo Native SDK 型や VarjoToolkit 型を CSV 行文字列へ変換
  - 型に対応した CSV ヘッダ文字列を生成
- `VarjoEyeTrackingService`
  - gaze / eye measurements / IPD / rendering gaze / projected gaze position の取得と CSV ログ出力
- `VarjoIMUService`
  - `varjo_WaitSync` / `varjo_FrameGetPose` による HMD pose サンプリングと CSV ログ出力
- `VarjoVSTService`
  - DistortedColor / VST の CPU NV12 stream を取得し、左右 MP4 動画と metadata CSV を出力
- `VarjoEyeCameraService`
  - EyeCamera stream の CPU buffer を左右 raw buffer と metadata CSV として出力
- `VarjoEnvironmentCubemapService`
  - EnvironmentCubemap stream の CPU buffer を raw buffer と metadata CSV として出力
- `VarjoTimestampMapping`
  - Varjo monotonic timestamp を Unix time に変換する軽量ユーティリティ

## ディレクトリ構成

```txt
VarjoToolkit/
  CMakeLists.txt
  include/
    VarjoToolkit/
      Core/
      DataStream/
      MR/
      Services/
        EyeTracking/
        IMU/
        VST/
        EyeCamera/
        Cubemap/
      Utilities/
  src/
  samples/
    ServiceLoggerSample/
  tests/
```

## 必要環境

- Windows
- Visual Studio 2022 など、C++17 対応 MSVC 環境
- CMake 3.20 以上
- Varjo Base / Varjo Runtime
- Varjo Native SDK
- Boost headers
  - 現在は `boost/circular_buffer.hpp` を使用
  - 未検出の場合は CMake が `FetchContent` で Boost headers を自動取得します
- `ffmpeg.exe`
  - `VarjoVSTService` で MP4 を出力する場合のみ必要

## ビルド方法

`VARJO_SDK_ROOT` には Varjo Native SDK のルートディレクトリを指定してください。Boost headers は未検出なら CMake が自動取得します。

```bat
set "VARJO_SDK_ROOT=C:\Program Files\Varjo\Varjo Native SDK"

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON

cmake --build out\build\default --config Release
```

オフライン環境や既存 Boost を使いたい場合だけ `BOOST_INCLUDE_DIR` を指定してください。

```bat
cmake -S . -B out\build\default ^
  -G "NMake Makefiles" ^
  -DVARJO_INCLUDE_DIR="%VARJO_SDK_ROOT%\include" ^
  -DVARJO_LIBRARY="%VARJO_SDK_ROOT%\lib\VarjoLib.lib" ^
  -DBOOST_INCLUDE_DIR="C:\path\to\boost" ^
  -DVARJOTOOLKIT_FETCH_BOOST=OFF ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=OFF ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON
```

## Scoped lock wrapper

`VarjoScopedLock` は `varjo_Lock` / `varjo_Unlock` の RAII wrapper です。

```cpp
#include <VarjoToolkit/Core/VarjoScopedLock.hpp>

{
    VarjoScopedLock lock(session, varjo_LockType_ChromaKey);
    if (!lock) {
        return;
    }

    // lock が有効な間だけ MR feature の設定を変更する。
}
// destructor で自動 unlock
```

## Chroma key wrapper

`VarjoChromaKey` は Varjo examples の chroma key 設定処理を低レイヤ wrapper として切り出したものです。設定変更時は内部で `VarjoScopedLock(varjo_LockType_ChromaKey)` を使います。

```cpp
#include <VarjoToolkit/MR/VarjoChromaKey.hpp>

VarjoChromaKey chroma(session);

// アプリケーションの layer に対して chroma key を有効化する。
chroma.setEnabled(true);

// HSV chroma key config を作成して slot 0 に設定する。
auto config = VarjoChromaKey::makeHSVConfig(
    0.33, 0.80, 0.60,   // target H,S,V
    0.08, 0.20, 0.20,   // tolerance H,S,V
    0.02, 0.05, 0.05);  // falloff H,S,V

chroma.setConfig(0, config);

// すべての config slot を無効化する。
chroma.disableAllConfigs();
```

`setGlobalEnabled(true)` はすべてのアプリケーション layer に強制的に chroma key を適用するため、通常は `setEnabled(true)` を使ってください。

## Camera property wrapper

`VarjoCameraProperties` は Varjo examples の `CameraManager` 相当の低レイヤ wrapper です。UI や logging を持たず、camera property の列挙・cache 更新・mode/value 設定を提供します。

```cpp
#include <VarjoToolkit/MR/VarjoCameraProperties.hpp>

VarjoCameraProperties camera(session);

// MR が利用可能な状態で camera property を列挙する。
camera.enumerate(true);

// Exposure / WhiteBalance を auto に戻す。
camera.setAutoMode(varjo_CameraPropertyType_ExposureTime);
camera.setAutoMode(varjo_CameraPropertyType_WhiteBalance);

// 手動値を設定する場合。
auto iso = VarjoCameraProperties::makeIntValue(200);
camera.setManualValue(varjo_CameraPropertyType_ISOValue, iso);

// UI 操作用に次の mode/value へ切り替える。
camera.applyNextModeOrValue(varjo_CameraPropertyType_ExposureTime);

// cache された状態を文字列化する。
std::string text = camera.propertyAsString(varjo_CameraPropertyType_ExposureTime);
```

`setMode` / `setValue` / `setManualValue` / `reset` / `resetAll` は、内部で `varjo_Lock(session, varjo_LockType_Camera)` と `varjo_Unlock` を使います。複数の変更をまとめたい場合は、明示的に `acquireLock()` / `releaseLock()` を使えます。

```cpp
if (camera.acquireLock()) {
    camera.setAutoMode(varjo_CameraPropertyType_ExposureTime);
    camera.setAutoMode(varjo_CameraPropertyType_WhiteBalance);
    camera.releaseLock();
}
```

## DataStream wrapper

`VarjoDataStream` は、stream config の列挙、条件指定による config 選択、start/stop、callback dispatch をまとめた低レイヤ wrapper です。

```cpp
#include <VarjoToolkit/DataStream/VarjoDataStream.hpp>
#include <VarjoToolkit/DataStream/VarjoDataStreamBufferLock.hpp>

VarjoDataStream stream(session);

VarjoDataStream::ConfigRequest request{};
request.streamType = varjo_StreamType_DistortedColor;
request.format = varjo_TextureFormat_NV12;
request.bufferType = varjo_BufferType_CPU;
request.requiredChannels = static_cast<varjo_ChannelFlag>(
    static_cast<int64_t>(varjo_ChannelFlag_Left) |
    static_cast<int64_t>(varjo_ChannelFlag_Right));

stream.startBest(request, [] (const varjo_StreamFrame* frame, varjo_Session* callbackSession) {
    if (!frame) {
        return;
    }

    const varjo_BufferId bufferId = varjo_GetBufferId(
        callbackSession,
        frame->id,
        frame->frameNumber,
        varjo_ChannelIndex_Left);

    VarjoDataStreamBufferLock lock(callbackSession, bufferId);
    if (!lock) {
        return;
    }

    const varjo_BufferMetadata metadata = lock.metadata();
    const void* cpuData = lock.cpuData();

    // metadata / cpuData をここでコピーする。
});
```

`VarjoDataStream` は move 禁止です。`varjo_StartDataStream` に渡す callback user_data がオブジェクトのアドレスを保持するため、stream 開始後にオブジェクトを移動できない設計にしています。

## CSV ユーティリティ

`include/VarjoToolkit/Utilities/VarjoCsv.hpp` を include すると、Varjo 型や Toolkit 型を CSV 行文字列へ変換できます。

```cpp
#include <VarjoToolkit/Utilities/VarjoCsv.hpp>

varjo_Vector3D coord{1.0, 2.0, 3.0};

std::string header = VarjoToolkit::Csv::header<varjo_Vector3D>("coord");
std::string row = VarjoToolkit::Csv::toCsv(coord);

// header: "coord.x,coord.y,coord.z"
// row:    "1,2,3"
```

任意の `x,y,z` 形式の独自 struct では、汎用 helper を使えます。

```cpp
struct dxyz {
    double x;
    double y;
    double z;
};

dxyz coord{1.0, 2.0, 3.0};

std::string header = VarjoToolkit::Csv::makeHeader("coord", {"x", "y", "z"});
std::string row = VarjoToolkit::Csv::join({
    VarjoToolkit::Csv::number(coord.x),
    VarjoToolkit::Csv::number(coord.y),
    VarjoToolkit::Csv::number(coord.z)
});
```

主な対応型:

- `varjo_Vector2Df`
- `varjo_Vector3D`
- `varjo_Matrix`
- `varjo_Matrix3x3`
- `varjo_Ray`
- `varjo_ViewInfo`
- `varjo_BufferMetadata`
- `varjo_CameraIntrinsics2`
- `varjo_StreamConfig`
- `varjo_StreamFrame`
- `varjo_Gaze`
- `varjo_EyeMeasurements`
- `varjo_Event`
- `varjo_WorldObject`
- `varjo_WorldObjectMarkerComponent`
- `varjo_Mesh2Df`
- `varjo_CameraPropertyValue`
- `VarjoProjectedGazePosition`
- `VarjoFrameInfoSnapshot`
- `VarjoCameraPropertyInfo`

## テスト

テストは `VARJOTOOLKIT_BUILD_TESTS=ON` で有効化します。Boost headers は未検出なら CMake が自動取得します。

```bat
set "VARJO_SDK_ROOT=C:\Program Files\Varjo\Varjo Native SDK"

cmake -S . -B out\build\test ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON ^
  -DVARJOTOOLKIT_BUILD_TESTS=ON

cmake --build out\build\test --config Release
ctest --test-dir out\build\test -C Release --output-on-failure
```

現在のテスト:

- `VarjoToolkitCoreSmokeTest`
  - Varjo Base と HMD 接続が必要
  - `VarjoSession` / `VarjoFrameInfo` の smoke test
- `VarjoToolkitCsvUtilityTest`
  - HMD 不要
  - CSV 文字列生成テスト
- `VarjoToolkitDataStreamWrappersTest`
  - HMD 不要
  - DataStream wrapper の config match / null session / invalid lock テスト
- `VarjoToolkitCameraPropertiesTest`
  - HMD 不要
  - camera property helper、value string、null session failure テスト
- `VarjoToolkitScopedLockChromaKeyTest`
  - HMD 不要
  - scoped lock / chroma key helper / null session failure テスト

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

古いサンプルやアプリケーションとの互換用に `VarjoServices` alias も定義されていますが、新しいコードでは `VarjoToolkit::VarjoToolkit` を使用してください。

## 注意点

- `VarjoSession` は default constructor で `varjo_SessionInit` を呼びます。
- `VarjoFrameInfo` は `varjo_FrameInfo*` を所有します。長期保存や queue には `VarjoFrameInfoSnapshot` を使ってください。
- `varjo_Session*` から構築した wrapper は session を所有しません。呼び出し側が session lifetime を保証してください。
- `std::shared_ptr<varjo_Session>` または `VarjoSession` から構築した wrapper は shared ownership を保持します。
- `VarjoScopedLock` は destructor で自動 unlock します。lock 対象の feature を長時間占有しないよう注意してください。
- `VarjoDataStream` は copy/move 禁止です。stream を開始した object の lifetime を stop 後まで維持してください。
- `VarjoDataStreamBufferLock` は callback 内で buffer をロックし、destructor で自動 unlock します。CPU data は callback 内で必要な分をコピーしてください。
- `VarjoCameraProperties` の set/reset 系 API は camera lock を必要とします。通常は内部で自動 lock/unlock します。
- `VarjoChromaKey` の config 設定 API は chroma key lock を必要とします。通常は内部で自動 lock/unlock します。
- CSV ユーティリティは値のエスケープを行いません。現在の対象は数値・bool・列挙値など、カンマを含まない値です。

## ライセンス

ライセンスファイルを追加していない場合、このリポジトリの利用条件は未定義です。公開利用する場合は `LICENSE` の追加を推奨します。
