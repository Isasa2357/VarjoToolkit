# Changelog

## Unreleased

### Added

- Dedicated HMD service performance benchmark suite selected with the CTest label `benchmark`.
  - Measures VST, eye camera, environment cubemap, IMU/head pose, and eye tracking.
  - Uses a 1-second warmup and 10-second measurement by default.
  - Reports one-second interval rates, min/average/max, cumulative observed average, received/processed/written counts, queue drops, and write failures where supported.
  - Measurement duration can be configured with `VARJOTOOLKIT_HMD_PERFORMANCE_SECONDS` and `VARJOTOOLKIT_HMD_PERFORMANCE_WARMUP_SECONDS`.
  - Added `docs/HMD_SERVICE_PERFORMANCE_TESTS.md`.
- Eye tracking application-queue drop statistics through `droppedSampleCount()`.

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
  - `VarjoD3D11RenderSample` renders a clear color and triangle through `VarjoSwapChain`, `VarjoMultiProjLayer`, and `VarjoLayerFrame`.
  - `VarjoD3D12RenderSample` renders a clear color and triangle through the D3D12 swapchain path.
- Video post process wrapper and tests.
  - `VarjoVideoPostProcessShader` wraps Varjo Native SDK experimental Video Post Process Shader API.
