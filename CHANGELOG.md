# Changelog

## v0.5.0 - External frame synchronization release - 2026-07-11

### Breaking changes

- Removed `VarjoFrameInfo::waitSync()` from the public API.
- `VarjoToolkit` services no longer call `varjo_WaitSync` internally.
- Changed `VarjoIMUService::start(bool)` to `VarjoIMUService::start()`.
- Eye tracking and IMU/head-pose services now require externally supplied frame snapshots through `submitFrameInfo()`.

### Added

- `VarjoFrameInfoSnapshot` now carries:
  - synchronized view information
  - display time and frame number
  - Center Pose from the same synchronized frame
  - explicit validity flags
- `VarjoEyeTrackingProvider::submitFrameInfo()` and `VarjoEyeTrackingService::submitFrameInfo()`.
- `VarjoIMUService::submitFrameInfo()` with a bounded asynchronous processing queue.
- Received, processed, written, and dropped external-frame counters for IMU logging.
- Test-only external synchronization pump used by HMD smoke and performance tests.

### Changed

- Rendering applications are the sole owners of `varjo_WaitSync`.
- Eye tracking stores a bounded history of externally supplied frame snapshots for timestamp matching and gaze projection.
- IMU/head-pose logging derives position, Euler angles, and angular velocity from the externally supplied Center Pose without another frame wait.
- D3D11/D3D12 and service logger samples now perform synchronization in the application and distribute snapshots to services.
- Bumped project and public version header to `0.5.0`.

## v0.4.0 - HMD service metrics and hardware test release - 2026-07-11

### Added

- Lightweight per-run sample-rate metrics for camera and sensor services.
  - VST and eye camera expose left/right received-frame counts and FPS.
  - Environment cubemap exposes received-frame count and FPS.
  - IMU/head pose and eye tracking expose received-sample counts and samples per second.
- Explicit received / processed / written / dropped / write-failure statistics where supported.
- Eye tracking application-queue drop statistics through `droppedSampleCount()`.
- Restart-safe counter utilities.
  - `VarjoDataStreamFrameQueue` now tracks total and per-channel receive counts and rates.
  - `VarjoToolkit::SampleRateCounter`, `RunCountingDeque`, and `RunResetSignal` reset per-run state when a service is restarted.
- HMD service smoke and restart tests for:
  - VST distorted-color camera
  - eye camera
  - environment cubemap
  - IMU/head pose
  - eye tracking
- Configurable restart-cycle testing through `VARJOTOOLKIT_HMD_RESTART_CYCLES`.
- Dedicated HMD service performance benchmark suite selected with the CTest label `benchmark`.
  - Uses a 1-second warmup and 10-second measurement by default.
  - Reports one-second interval rates, min/average/max, cumulative observed average, received/processed/written counts, queue drops, and write failures where supported.
  - Measurement duration can be configured with `VARJOTOOLKIT_HMD_PERFORMANCE_SECONDS` and `VARJOTOOLKIT_HMD_PERFORMANCE_WARMUP_SECONDS`.
- HMD-independent tests for rate calculation, per-channel counting, queue overflow accounting, and restart resets.
- Documentation:
  - `docs/HMD_SERVICE_METRICS.md`
  - `docs/HMD_SERVICE_PERFORMANCE_TESTS.md`

### Changed

- Camera and sensor FPS getters now consistently represent successfully received samples rather than writer-thread completion.
- Eye tracking rate calculation no longer depends on the current application queue contents, so `requestData()` does not reset the measured rate.
- HMD tests use the CTest resource lock `varjo_hmd` to prevent concurrent access to the same headset.
- Optional eye camera, cubemap, and eye tracking tests return CTest `Skipped` when the connected hardware, runtime permission, or calibration does not provide the feature.
- Service restart tests use a 120-second timeout and the `restart` label.
- Bumped the project and public version header to `0.4.0`.

### Verified

- Core, rendering, D3D11 swapchain, and D3D12 swapchain HMD smoke tests passed in the developer hardware environment.
- VST, eye camera, environment cubemap, IMU, and eye tracking service smoke/restart tests passed.
- The five dedicated HMD service performance benchmarks passed.

## v0.3.0 - SuperDebug diagnostics release

### Added

- SuperDebug console diagnostics mode.
  - Added `VARJOTOOLKIT_ENABLE_SUPERDEBUG` CMake option.
  - Enables `VARJOTOOLKIT_SUPERDEBUG=1` only for Debug configuration when the option is ON.
  - Added header-only diagnostics macros in `VarjoToolkit/Diagnostics/VarjoDiagnostics.hpp`.
  - Default SuperDebug output is limited to external API boundaries, failures, and important state changes.
  - `VTK_SD_TRACE` and `VTK_SD_SCOPE` are no-op by default to avoid noisy frame/callback logs and keep normal debugging output readable.
  - Added console SuperDebug logging to session, frame info, scoped locks, event queue, data stream, stream buffer lock, chroma key, world, marker tracker, occlusion mesh, swapchain, layer frame, and video post process wrappers.
  - Added console SuperDebug logging to event, marker tracking, IMU, VST, eye camera, environment cubemap, and eye tracking services, including CSV/raw output open/write/close, worker lifecycle, queue/buffer overflow, dropped data, and final summary logs.
  - Added `docs/SUPERDEBUG.md`.
- Expanded HMD smoke test suite under `VARJOTOOLKIT_BUILD_HMD_TESTS`.
  - Added occlusion mesh, event queue, world/marker, MR camera property, data stream config, D3D11 swapchain, and D3D12 swapchain HMD tests.
  - Added experimental D3D11/D3D12 Video Post Process Shader HMD tests when `VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=ON`.
  - Added labels for targeted HMD runs: `hmd`, `mr`, `datastream`, `rendering`, `d3d11`, `d3d12`, `experimental`, and `vpp`.

### Changed

- Bumped project and public version header to `0.3.0`.

## v0.2.0 - Rendering, package, and video post process release

### Added

- CMake install / package support.
  - Installs the `VarjoToolkit` static library and public headers.
  - Exports `VarjoToolkit::VarjoToolkit` through `VarjoToolkitTargets.cmake`.
  - Generates `VarjoToolkitConfig.cmake` and `VarjoToolkitConfigVersion.cmake` for `find_package(VarjoToolkit CONFIG REQUIRED)`.
- Package consumer test.
  - Verifies that an external CMake project can use `find_package(VarjoToolkit CONFIG REQUIRED)` and link `VarjoToolkit::VarjoToolkit`.
- HMD-required test split.
  - Added `VARJOTOOLKIT_BUILD_HMD_TESTS`.
  - `VarjoToolkitCoreSmokeTest` is now registered only when HMD tests are enabled.
- `scripts/run_coverage_open_cpp_coverage.bat`.
  - Builds Debug tests with Visual Studio generator.
  - Runs HMD-independent tests through OpenCppCoverage.
  - Exports HTML and Cobertura XML reports under `out/coverage`.
- `samples/CMakeLists.txt`.
  - Central sample entry point for all sample projects.
- D3D rendering samples.
  - `VarjoD3D11RenderSample` renders a clear color through `VarjoSwapChain`, `VarjoMultiProjLayer`, and `VarjoLayerFrame`.
  - `VarjoD3D12RenderSample` renders a clear color through the D3D12 swapchain path.
- Video post process wrapper and tests.
  - `VarjoVideoPostProcessShader` wraps Varjo Native SDK experimental Video Post Process Shader API.
