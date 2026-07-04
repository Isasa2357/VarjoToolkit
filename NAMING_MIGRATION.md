# VarjoServices naming migration

## File layout

```text
include/VarjoServices/
  EyeTracking/VarjoEyeTrackingService.hpp
  VST/VarjoVSTService.hpp
  IMU/VarjoIMUService.hpp
src/
  VarjoEyeTrackingService.cpp
  VarjoVSTService.cpp
  VarjoIMUService.cpp
  VarjoVSTService.cpp
```

`TimeMapping` は削除済みの前提です。

## Main renames

| Old name | New name |
|---|---|
| `VSTService` | `VarjoVSTService` |
| `VarjoEyeTrackerService` | `VarjoEyeTrackingService` |
| `VarjoEyeTracker` | `VarjoEyeTrackingProvider` |
| `VarjoEyeTrackerData` | `VarjoEyeTrackingData` |
| `VarjoEyeTrackerDataLogger` | `VarjoEyeTrackingDataLogger` |
| `VarjoIMUService::VarjoIMUInfo` | `VarjoIMUService::VarjoIMUData` |

## Public method renames

| Old name | New name |
|---|---|
| `end()` | `stop()` |
| `requestEyeTrackerData()` | `requestData()` |
| `is_running()` | `isRunning()` |
| `last_error()` | `lastError()` |
| `row_count()` | `rowCount()` |
| `output_path()` | `outputPath()` |
| `buffer_capacity()` | `bufferCapacity()` |
| `buffer_size()` | `bufferSize()` |
| `latest_info()` | `latestData()` |
| `left_frame_count()` | `leftFrameCount()` |
| `right_frame_count()` | `rightFrameCount()` |
| `dropped_frame_count()` | `droppedFrameCount()` |
| `write_failure_count()` | `writeFailureCount()` |

## Sample

`samples/ServiceLoggerSample` は、リネーム後の API 名で次の Service を起動します。

- `VarjoEyeTrackingService`
- `VarjoIMUService`
- `VarjoVSTService`

VST 動画保存には `ffmpeg.exe` が必要です。VST を使わない場合は `--no-vst` を指定してください。
