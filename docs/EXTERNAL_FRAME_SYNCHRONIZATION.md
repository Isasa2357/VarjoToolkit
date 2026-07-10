# External Frame Synchronization

VarjoToolkit 0.5.0以降では、`VarjoToolkit`のクラスとサービスは`varjo_WaitSync`を呼びません。
フレーム同期はレンダラーなどのアプリケーション側コンポーネントが1か所だけで所有します。

## 理由

同じ`varjo_Session`に対してレンダラー、Eye Tracking、IMUがそれぞれ`varjo_WaitSync`を呼ぶと、各呼び出し元が異なる表示同期を受け取ります。90 HzのHMDで3か所が同期を待つと、レンダラーが約30 fpsになる構成が発生します。

## VarjoFrameInfo

`VarjoFrameInfo`は`varjo_FrameInfo`の確保、参照、snapshot化、解放だけを担当します。
`waitSync()`メンバー関数はありません。

低レベルアプリケーションでは、同期所有者がRaw Varjo APIを呼びます。

```cpp
VarjoSession session;
VarjoFrameInfo frameInfo(session);

varjo_WaitSync(session.get(), frameInfo.get());
const VarjoFrameInfoSnapshot snapshot = frameInfo.snapshot();
```

`VarjoFrameInfoSnapshot`には、同一同期フレームの次の値が含まれます。

- `views`
- `displayTime`
- `frameNumber`
- `centerPose`
- `centerPoseValid`
- `valid`

## Eye Tracking

```cpp
VarjoEyeTrackingService eyeService(
    session.shared(),
    VarjoEyeTrackingProvider::OutputFilterType::NONE,
    VarjoEyeTrackingProvider::OutputFrequency::MAXIMUM,
    "eye_tracking.csv");

eyeService.start();
eyeService.submitFrameInfo(snapshot);
```

Eye Trackingは外部snapshotを時系列履歴として保持し、Gazeの`captureTime`に対応するFrameInfoを選択して投影計算とCSV出力に使用します。Gazeの取得ワーカーは残りますが、FrameInfo取得用の同期ワーカーはありません。

## IMU / Head Pose

```cpp
VarjoIMUService imuService(
    session.shared(),
    L"imu.csv",
    512);

imuService.start();
imuService.submitFrameInfo(snapshot);
```

IMUサービスはsnapshotの`centerPose`から位置、Euler角、角速度を計算します。投入は軽量なキュー操作であり、計算とCSV書き込みはサービスワーカーで行います。

## VarjoXRとの組み合わせ

VarjoXR 0.2.0以降はD3D11/D3D12バックエンドが唯一の同期所有者です。

```cpp
space.update();

const VarjoFrameInfoSnapshot snapshot = space.frameInfoSnapshot();
eyeService.submitFrameInfo(snapshot);
imuService.submitFrameInfo(snapshot);
```

`frameInfoSnapshot()`は、バックエンドが`varjo_WaitSync`を実行した直後に保存したsnapshotを返します。呼び出し時に新しい同期処理は発生しません。

## 禁止する構成

次のようにサービスごとに`varjo_WaitSync`を呼ばないでください。

```text
render thread  -> varjo_WaitSync
Eye worker     -> varjo_WaitSync  // 禁止
IMU worker     -> varjo_WaitSync  // 禁止
```

同期所有者はアプリケーション全体で1つにします。
