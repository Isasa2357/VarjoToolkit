# Service performance metrics

VarjoToolkit のカメラ・センサ系サービスは、現在の実行区間における受信数と実効サンプルレートを取得できます。

## 定義

### Received

SDK からバッファを正常に取得し、サービス内部のキューへ投入できたサンプル数です。
キュー容量超過で後から破棄されたフレームも Received には含まれます。

### Processed

writer worker がキューから取り出して処理したサンプル数です。
既存の `leftFrameCount()`、`rightFrameCount()`、`frameCount()` は互換性維持のため Processed の意味で残しています。

### Written

Processed から `writeFailureCount()` を引いた、正常書き込み数です。
VST と Eye Camera は左右合計値、Cubemap は単一ストリームの値を返します。

### Dropped

内部キューの容量超過により、writer worker へ到達する前に破棄されたサンプル数です。
正常停止後は、通常次の関係になります。

```text
Received = Processed + Dropped
Written  = Processed - WriteFailures
```

## 公開 API

### VST

```cpp
uint64_t leftReceivedFrameCount() const;
uint64_t rightReceivedFrameCount() const;
uint64_t receivedFrameCount() const;
uint64_t leftProcessedFrameCount() const;
uint64_t rightProcessedFrameCount() const;
uint64_t processedFrameCount() const;
uint64_t writtenFrameCount() const;
double getLeftFramesPerSecond() const;
double getRightFramesPerSecond() const;
```

### Eye Camera

VST と同じカウンタ構成です。

### Environment Cubemap

```cpp
uint64_t receivedFrameCount() const;
uint64_t processedFrameCount() const;
uint64_t writtenFrameCount() const;
double getFramesPerSecond() const;
```

### IMU

IMU は取得と CSV 書き込みを同じ worker で同期的に行うため、正常サンプルについて Received と Written は同じです。

```cpp
uint64_t receivedSampleCount() const;
uint64_t writtenSampleCount() const;
double getSamplesPerSecond() const;
```

### Eye Tracking

Eye Tracking のカウンタは `requestData()` でキューを空にしても減少しません。
現在の実行区間で、取得・座標変換・logger write・application queue への投入まで完了したサンプルを数えます。

```cpp
uint64_t receivedSampleCount() const;
uint64_t processedSampleCount() const;
uint64_t writtenSampleCount() const;
double getSamplesPerSecond() const;
```

## サンプルレート

レート getter は直前の確定計測から約1秒以上経過した時点で、累積サンプル数の増分を経過時間で割って更新します。
1秒未満で再度呼び出した場合は、直前の確定値を返します。

各サービスの `start()` で新しい実行区間が開始され、受信数とレート測定値はリセットされます。

## 再起動テスト

HMD service tests は、既定で同じサービスインスタンスを3回 `start()` / `stop()` します。
回数は環境変数で変更できます。

```bat
set "VARJOTOOLKIT_HMD_RESTART_CYCLES=10"
ctest --test-dir out\build\hmd_tests -C Debug --output-on-failure -L "restart"
```

許容範囲は1回から20回です。

HMD不要のカウンタ単体テストは次で実行できます。

```bat
ctest --test-dir out\build\hmd_tests -C Debug --output-on-failure -R VarjoToolkitSampleRateUtilitiesTest
```
