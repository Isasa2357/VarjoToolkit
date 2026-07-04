# ServiceLoggerSample

`VarjoServices` の各 Service を同時に起動し、CSV / 動画ログを出力する最小サンプルです。

## 実行内容

- `VarjoTimeMappingService`
  - `time_mapping.csv` を出力します。
- `VarjoEyeTrackingService`
  - `eye_tracking.csv` を出力します。
  - `requestData()` の使い方も示します。
- `VarjoIMUService`
  - `imu.csv` を出力します。
  - `latestData()` と `rowCount()` の使い方も示します。
- `VarjoVSTService`
  - `sample_vst_left.mp4`
  - `sample_vst_right.mp4`
  - `sample_vst_left_metadata.csv`
  - `sample_vst_right_metadata.csv`
  - VST 動画保存には `ffmpeg.exe` が必要です。

## ビルド例

CMD プロンプトでリポジトリルートから実行します。

```bat
set "VARJO_SDK_ROOT=C:\Program Files\Varjo\varjo_native_sdk"
rmdir /s /q out\build\default 2>nul
cmake -S . -B out\build\default -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" -DVARJOSERVICES_BUILD_SAMPLES=ON
cmake --build out\build\default --config Release
```

Varjo SDK の配置が異なる場合は、`VARJO_SDK_ROOT` を自分の環境に合わせてください。

## 実行例

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out logs --seconds 30
```

`ffmpeg.exe` が PATH にない場合、または VST 動画保存を試さない場合は次のように実行します。

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out logs --seconds 30 --no-vst
```

## オプション

```text
--out <dir>      出力ディレクトリ。既定値は varjo_service_logs
--seconds <n>    ログ時間。0 を指定すると Ctrl+C まで継続
--no-eye         VarjoEyeTrackingService を無効化
--no-imu         VarjoIMUService を無効化
--no-time        VarjoTimeMappingService を無効化
--no-vst         VarjoVSTService を無効化
--help           ヘルプ表示
```

## 注意

- EyeTracking を使うには、Varjo Base 側で視線取得が許可され、キャリブレーション済みである必要があります。
- VST 動画保存は内部で `ffmpeg` に raw NV12 を渡して MP4 化します。
- `VarjoEyeTrackingService` のログパスは現在 `std::string` で受けるため、サンプルの `--out` には ASCII のパスを推奨します。
