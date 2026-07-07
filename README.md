# VarjoToolkit

VarjoToolkit は、Varjo Native SDK を使う C++ アプリケーションから、視線、HMD pose / IMU 相当情報、VST カメラ、EyeCamera、EnvironmentCubemap、Varjo timestamp 変換を扱いやすくするための小さな C++17 ライブラリです。

このリポジトリは、Varjo の各種データ取得処理をサービスクラスとしてまとめ、サンプルアプリケーションから CSV、動画、raw buffer として記録できるようにすることを目的としています。

## 主な機能

- `VarjoSession`
  - `varjo_SessionInit` / `varjo_SessionShutDown` の RAII wrapper
  - 既存サービスに渡しやすい `std::shared_ptr<varjo_Session>` を保持
- `VarjoFrameInfo`
  - `varjo_CreateFrameInfo` / `varjo_FreeFrameInfo` / `varjo_WaitSync` の RAII wrapper
  - `varjo_Session*`、`std::shared_ptr<varjo_Session>`、`VarjoSession` から構築可能
  - `std::shared_ptr<varjo_Session>` または `VarjoSession` から構築した場合は session ownership を保持
  - 保存やキュー投入に使える `VarjoFrameInfoSnapshot` を提供
- `VarjoDataStream`
  - `varjo_GetDataStreamConfigs` / `varjo_StartDataStream` / `varjo_StopDataStream` の wrapper
  - stream config の列挙、条件指定による best config 選択、callback dispatch を提供
  - `varjo_Session*`、`std::shared_ptr<varjo_Session>`、`VarjoSession` から構築可能
- `VarjoDataStreamBufferLock`
  - `varjo_LockDataStreamBuffer` / `varjo_UnlockDataStreamBuffer` の RAII wrapper
  - DataStream callback 内の buffer lock/unlock 漏れを防止
- `VarjoToolkit::Csv`
  - Varjo Native SDK 型や VarjoToolkit 型を CSV 行文字列へ変換
  - 型に対応した CSV ヘッダ文字列を生成
  - 任意の `x,y,z` 形式 struct などに使える汎用 header / join helper を提供
- `VarjoEyeTrackingService`
  - Varjo gaze と eye measurements の取得
  - user IPD / HMD IPD / IPD adjustment mode の取得
  - rendering gaze の取得
  - gaze に対応する `varjo_FrameInfo` を保持し、視線位置を Varjo display 座標および video/image UV 座標へ変換
  - CSV ログ出力
- `VarjoIMUService`
  - `varjo_WaitSync` / `varjo_FrameGetPose` による HMD pose サンプリング
  - position、Euler 角、角速度の CSV ログ出力
- `VarjoVSTService`
  - Varjo DataStream API から distorted color / VST の CPU NV12 stream を取得
  - 左右 MP4 動画と metadata CSV を出力
  - 動画書き出しには `ffmpeg` を使用
- `VarjoEyeCameraService`
  - EyeCamera stream の CPU buffer を取得
  - 左右 raw buffer と metadata CSV を出力
- `VarjoEnvironmentCubemapService`
  - EnvironmentCubemap stream の CPU buffer を取得
  - raw buffer と metadata CSV を出力
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
- `ffmpeg.exe`
  - `VarjoVSTService` で MP4 を出力する場合のみ必要
  - `ffmpeg.exe` を `PATH` に追加してください

## 使用ライブラリ

- Varjo Native SDK
- Boost headers

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

Boost が自動検出できない場合は、CMake の include search path から見える場所に Boost headers を置くか、CMake 側の探索設定を追加してください。

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
    // unlock は VarjoDataStreamBufferLock の destructor で自動実行される。
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

現在は主に以下に対応しています。

- `varjo_Vector2Df`
- `varjo_Vector3D`
- `varjo_Matrix`
- `varjo_Matrix3x3`
- `varjo_Ray`
- `varjo_ViewInfo`
- `varjo_BufferMetadata`
- `varjo_CameraIntrinsics2`
- `varjo_Gaze`
- `varjo_EyeMeasurements`
- `VarjoProjectedGazePosition`
- `VarjoFrameInfoSnapshot`

## テスト

テストは `VARJOTOOLKIT_BUILD_TESTS=ON` で有効化します。

`VarjoToolkitCoreSmokeTest` は、Varjo Base が起動しており、Varjo HMD が接続されている前提の smoke test です。`VarjoSession` の初期化、`VarjoFrameInfo` の作成、`varjo_WaitSync`、snapshot 取得、`std::shared_ptr<varjo_Session>` 互換、move ownership を確認します。

`VarjoToolkitCsvUtilityTest` は CSV 文字列生成の HMD 不要テストです。Varjo SDK のヘッダとライブラリは必要ですが、Varjo runtime 起動や HMD 接続は不要です。

`VarjoToolkitDataStreamWrappersTest` は DataStream wrapper の HMD 不要テストです。config match / scoring、null session start failure、invalid buffer lock を確認します。

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

## サンプル実行

ビルド後、`VarjoServiceLoggerSample.exe` を実行すると、各サービスのログ出力を開始します。

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out varjo_service_logs --seconds 30
```

### オプション

```txt
--out <dir>          出力ディレクトリ。既定値: varjo_service_logs
--seconds <n>        ログ時間。0 の場合は Ctrl+C まで継続。既定値: 30
--no-eye            VarjoEyeTrackingService を無効化
--no-imu            VarjoIMUService を無効化
--no-vst            VarjoVSTService を無効化
--no-eye-camera     VarjoEyeCameraService を無効化
--no-cubemap        VarjoEnvironmentCubemapService を無効化
--no-timestamp      timestamp mapping CSV 出力を無効化
--help              ヘルプ表示
```

VST 動画出力には `ffmpeg.exe` が必要です。`ffmpeg` を使わない場合は `--no-vst` を指定してください。

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out varjo_service_logs --seconds 30 --no-vst
```

## 出力例

`ServiceLoggerSample` の既定設定では、出力ディレクトリに次のようなファイルが作成されます。

```txt
varjo_service_logs/
  eye_tracking.csv
  imu.csv
  timestamp_mapping.csv
  sample_vst_left.mp4
  sample_vst_right.mp4
  sample_vst_left_metadata.csv
  sample_vst_right_metadata.csv
  sample_eye_camera_left.raw
  sample_eye_camera_right.raw
  sample_eye_camera_left_metadata.csv
  sample_eye_camera_right_metadata.csv
  sample_environment_cubemap.raw
  sample_environment_cubemap_metadata.csv
```

利用可能な stream は Varjo Base の設定、接続している HMD、ライセンス、実行環境に依存します。利用できない stream はサンプル実行時に開始失敗として表示されます。

## CMake から利用する

このリポジトリをサブディレクトリとして追加する場合は、次のようにリンクします。

```cmake
add_subdirectory(path/to/VarjoToolkit)

target_link_libraries(YourApp
    PRIVATE
        VarjoToolkit::VarjoToolkit
)
```

古いサンプルやアプリケーションとの互換用に `VarjoServices` alias も定義されていますが、新しいコードでは `VarjoToolkit::VarjoToolkit` を使用してください。

## 注意点

- `VarjoSession` は default constructor で `varjo_SessionInit` を呼びます。遅延初期化したい場合は、将来的に別 factory を追加してください。
- `VarjoFrameInfo` は `varjo_FrameInfo*` を所有します。長期保存や queue には `VarjoFrameInfoSnapshot` を使ってください。
- `VarjoFrameInfo` は `std::shared_ptr<varjo_Session>` から構築した場合、その shared ownership を保持します。既存サービスが持つ session handle と安全に併用できます。
- `varjo_Session*` から構築した `VarjoFrameInfo` は session を所有しません。呼び出し側が session lifetime を保証してください。
- `VarjoDataStream` は copy/move 禁止です。stream を開始した object の lifetime を stop 後まで維持してください。
- `VarjoDataStreamBufferLock` は DataStream callback 内で buffer をロックし、destructor で自動 unlock します。CPU data は callback 内で必要な分をコピーしてください。
- CSV ユーティリティは値のエスケープを行いません。現在の対象は数値・bool・列挙値など、カンマを含まない値です。
- `VarjoEyeTrackingService` は `varjo_WaitSync` で取得した frame info を内部バッファに保持し、gaze の `captureTime` に対応する frame info を使って座標変換します。
- `VarjoIMUService` も `varjo_WaitSync` を使用します。レンダリングやカメラ処理への影響を抑えるため、worker thread は低優先度で動作します。
- DataStream 系サービスでは、Varjo callback 内では CPU buffer のコピーだけを行い、ファイル書き出しは別 thread で行います。
- VST / EyeCamera / EnvironmentCubemap の出力は大きくなるため、ログ出力ディレクトリは Git 管理対象から除外することを推奨します。

## ライセンス

ライセンスファイルを追加していない場合、このリポジトリの利用条件は未定義です。公開利用する場合は `LICENSE` の追加を推奨します。
