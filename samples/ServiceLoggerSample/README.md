# ServiceLoggerSample

`VarjoToolkit` の Service / Utilities を同時に起動し、CSV / 動画 / raw buffer ログを出力する最小サンプルです。

`VarjoTimeMappingService` のような TimeMapping Service は使いません。代わりに、`Utilities/VarjoTimestampMapping` を使って timestamp mapping CSV をサンプル側で出力します。

## 実行内容

- `VarjoEyeTrackingService`
  - `eye_tracking.csv` を出力します。
  - `requestData()` の使い方も示します。
- `VarjoIMUService`
  - `imu.csv` を出力します。
  - `latestData()` と `rowCount()` の使い方も示します。
- `VarjoTimestampMapping`
  - `timestamp_mapping.csv` を出力します。
  - Service ではなく、Utilities 配下の軽量な timestamp 変換クラスです。
- `VarjoVSTService`
  - `sample_vst_left.mp4`
  - `sample_vst_right.mp4`
  - `sample_vst_left_metadata.csv`
  - `sample_vst_right_metadata.csv`
  - VST 動画保存には `ffmpeg.exe` が必要です。
- `VarjoEyeCameraService`
  - `sample_eye_camera_left.raw`
  - `sample_eye_camera_right.raw`
  - `sample_eye_camera_left_metadata.csv`
  - `sample_eye_camera_right_metadata.csv`
  - EyeCamera の CPU buffer を padding 込みでそのまま raw 追記保存します。
- `VarjoEnvironmentCubemapService`
  - `sample_environment_cubemap.raw`
  - `sample_environment_cubemap_metadata.csv`
  - EnvironmentCubemap の CPU buffer を padding 込みでそのまま raw 追記保存します。

## ビルド例

CMD プロンプトでリポジトリルートから実行します。

```bat
set "VARJO_SDK_ROOT=C:\Program Files\Varjo\varjo_native_sdk"
rmdir /s /q out\build\default 2>nul
cmake -S . -B out\build\default -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" -DVARJOTOOLKIT_BUILD_SAMPLES=ON
cmake --build out\build\default --config Release
```

Varjo SDK の配置が異なる場合は、`VARJO_SDK_ROOT` を自分の環境に合わせてください。旧オプション名の `VARJOSERVICES_BUILD_SAMPLES` も互換用に受け付けます。

## 実行例

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out logs --seconds 30
```

`ffmpeg.exe` が PATH にない場合、または VST 動画保存を試さない場合は次のように実行します。

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out logs --seconds 30 --no-vst
```

EyeCamera / EnvironmentCubemap は Varjo Base のライセンスや機種側の capability に依存します。使わない場合は次のように無効化できます。

```bat
out\build\default\samples\ServiceLoggerSample\Release\VarjoServiceLoggerSample.exe --out logs --seconds 30 --no-eye-camera --no-cubemap
```

## オプション

```text
--out <dir>          出力ディレクトリ。既定値は varjo_service_logs
--seconds <n>        ログ時間。0 を指定すると Ctrl+C まで継続
--no-eye             VarjoEyeTrackingService を無効化
--no-imu             VarjoIMUService を無効化
--no-vst             VarjoVSTService を無効化
--no-eye-camera      VarjoEyeCameraService を無効化
--no-cubemap         VarjoEnvironmentCubemapService を無効化
--no-timestamp       VarjoTimestampMapping による timestamp_mapping.csv 出力を無効化
--help               ヘルプ表示
```

## 出力例

```text
logs/
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

## 注意

- EyeTracking を使うには、Varjo Base 側で視線取得が許可され、キャリブレーション済みである必要があります。
- VST 動画保存は内部で `ffmpeg` に raw NV12 を渡して MP4 化します。
- EyeCamera / EnvironmentCubemap は raw buffer をそのまま連結保存します。各フレームの `raw_byte_offset` / `raw_byte_size` / `buffer_format` / `buffer_row_stride` / `buffer_width` / `buffer_height` は metadata CSV を参照してください。
- `VarjoTimestampMapping` は自前のスレッドやログ所有を持ちません。このサンプルでは、進捗表示の1秒周期で `sampleCurrentMapping()` を呼び、CSVへ書き込んでいます。
- `VarjoEyeTrackingService` のログパスは現在 `std::string` で受けるため、サンプルの `--out` には ASCII のパスを推奨します。
