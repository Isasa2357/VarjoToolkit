# HMD Service Metrics

VarjoToolkit のカメラ／センサ系サービスは、実行中の取得性能を軽量に確認するための累積カウンタとレート getter を提供します。

## 基本方針

レートは「SDK またはセンサから正常に受信し、サービス内の処理対象として受け付けたサンプル数／秒」を表します。

- 取得ループや DataStream callback に専用の計測スレッドは追加しません。
- 既存の累積受信数との差分から、およそ1秒ごとのレートを計算します。
- 1秒未満の間隔で getter を呼んだ場合は直前の確定値を返します。
- 同じサービスインスタンスを `stop()` 後に再度 `start()` すると、実行区間のカウンタとレートは0へリセットされます。

## VST / Eye Camera

```cpp
uint64_t leftReceivedFrameCount() const;
uint64_t rightReceivedFrameCount() const;
uint64_t receivedFrameCount() const;

uint64_t leftProcessedFrameCount() const;
uint64_t rightProcessedFrameCount() const;
uint64_t processedFrameCount() const;

uint64_t writtenFrameCount() const;
uint64_t droppedFrameCount() const;
uint64_t writeFailureCount() const;

double getLeftFramesPerSecond() const;
double getRightFramesPerSecond() const;
```

各値の意味:

- `Received`: 有効なCPUバッファを取得し、内部キューへ投入したフレーム数
- `Processed`: writer thread まで到達したフレーム数
- `Dropped`: 内部キュー容量超過でwriter threadへ届く前に破棄されたフレーム数
- `Written`: 正常に動画／RAWへ書き込めたフレーム数
- `WriteFailures`: writer threadで書き込みに失敗したフレーム数

正常終了後は原則として次の関係が成立します。

```text
Received = Processed + Dropped
Written  = Processed - WriteFailures
```

既存の `leftFrameCount()` / `rightFrameCount()` は互換性維持のため残されており、`Processed` と同じ意味です。

## Environment Cubemap

```cpp
uint64_t receivedFrameCount() const;
uint64_t processedFrameCount() const;
uint64_t writtenFrameCount() const;
uint64_t droppedFrameCount() const;
uint64_t writeFailureCount() const;

double getFramesPerSecond() const;
```

意味と整合条件はVST／Eye Cameraと同じです。

## IMU / Head Pose

```cpp
uint64_t receivedSampleCount() const;
uint64_t writtenSampleCount() const;
double getSamplesPerSecond() const;
```

IMUはサンプリングとCSV書き込みを同一workerで同期的に行うため、正常サンプルについて `Received` と `Written` は同一です。

## Eye Tracking

```cpp
uint64_t receivedSampleCount() const;
uint64_t processedSampleCount() const;
uint64_t writtenSampleCount() const;
uint64_t droppedSampleCount() const;

double getSamplesPerSecond() const;
```

- `receivedSampleCount()` は現在の実行区間で取得・変換・記録・アプリケーションキュー投入まで完了した視線サンプル数です。
- `requestData()` でキューを空にしても累積受信数とレートは維持されます。
- `droppedSampleCount()` は bounded application queue が満杯になり、古いサンプルを削除した回数です。
- `requestData()` による通常の取り出しはドロップとして数えません。

## 使用例

```cpp
std::cout
    << "VST L/R: "
    << vstService.getLeftFramesPerSecond() << " / "
    << vstService.getRightFramesPerSecond() << " fps\n"
    << "VST received/processed/written/dropped: "
    << vstService.receivedFrameCount() << " / "
    << vstService.processedFrameCount() << " / "
    << vstService.writtenFrameCount() << " / "
    << vstService.droppedFrameCount() << '\n'
    << "IMU: " << imuService.getSamplesPerSecond() << " samples/s\n"
    << "Gaze: " << eyeTrackingService.getSamplesPerSecond() << " samples/s\n";
```

## 関連テスト

短時間の起動・停止・再起動テスト:

```bat
set "VARJOTOOLKIT_HMD_RESTART_CYCLES=3"
ctest --test-dir out\build\hmd_tests -C Debug --output-on-failure -L "service"
```

10秒性能計測:

```bat
set "VARJOTOOLKIT_HMD_PERFORMANCE_WARMUP_SECONDS=1"
set "VARJOTOOLKIT_HMD_PERFORMANCE_SECONDS=10"
ctest --test-dir out\build\hmd_tests -C Release -V -L "benchmark"
```

詳しい性能試験の説明は [`HMD_SERVICE_PERFORMANCE_TESTS.md`](HMD_SERVICE_PERFORMANCE_TESTS.md) を参照してください。
