# Changelog

## Unreleased

### Added

- Expanded HMD smoke test suite under `VARJOTOOLKIT_BUILD_HMD_TESTS`.
  - Added occlusion mesh, event queue, world/marker, MR camera property, data stream config, D3D11 swapchain, and D3D12 swapchain HMD tests.
  - Added experimental D3D11/D3D12 Video Post Process Shader HMD tests when `VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS=ON`.
  - Added labels for targeted HMD runs: `hmd`, `mr`, `datastream`, `rendering`, `d3d11`, `d3d12`, `experimental`, and `vpp`.

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
  - Accepts compiled shader bytecode as `const void* + size`; it does not compile HLSL source.
  - Accepts constant buffer updates as raw bytes and provides trivially-copyable struct template helpers.
  - Holds `varjo_LockType_VideoPostProcessShader` through RAII.
  - Supports D3D11 and D3D12 configure entry points with native DirectX objects supplied by the caller.
  - Provides supported texture format query helpers.
  - `VarjoShaderTextureLock` wraps `varjo_MRAcquireShaderTexture` / `varjo_MRReleaseShaderTexture` through RAII.
  - `VarjoToolkitVideoPostProcessShaderTest` covers bytecode views, config helpers, texture config helpers, null-session failure paths, and template constant buffer helpers.
- Video post process samples.
  - `VarjoVideoPostProcessD3D11Sample` demonstrates D3D11 Video Post Process Shader setup.
  - `VarjoVideoPostProcessD3D12Sample` demonstrates D3D12 Video Post Process Shader setup.
  - Both samples compile an embedded HLSL compute shader in the sample, pass the compiled blob to the wrapper, submit a trivially-copyable user constant buffer, and apply a circular VST dimming/highlight effect.
- `VarjoOcclusionMeshDumpSample`.
  - Dumps Varjo occlusion meshes for all runtime views.
  - Writes vertex CSV and OBJ triangle-list files.
  - Supports clockwise and counter-clockwise winding selection.
- `VarjoEventService`.
  - Polls `VarjoEventQueue` in a worker thread.
  - Writes event rows to CSV with row index, readable event type name, and `varjo_Event` payload fields.
  - Provides a bounded in-memory queue for application-side event consumption.
- `VarjoMarkerTrackingService`.
  - Samples `VarjoMarkerTracker` in a worker thread.
  - Writes marker object/component/pose rows to CSV.
  - Provides a bounded in-memory queue for application-side marker consumption.
- `VarjoToolkitEventMarkerServicesTest`.
  - HMD-independent tests for event/marker CSV row formatting, file output, and null-session service failure paths.
- `ServiceLoggerSample` integration for event and marker logging.
  - Added `--no-event` and `--no-marker` options.

### Changed

- Bumped project and public version header to `0.2.0`.
- Public include paths now use build/install generator expressions so the target can be exported cleanly.
- Documented install/package, testing, coverage, architecture, and video post process policies.
- Clarified that VarjoToolkit core must not depend on D3DHelper / D3D11Helper / D3D12Helper, OpenCV, JSON libraries, camera SDK wrappers, UI frameworks, or ML frameworks.
- Clarified that native DirectX SDK boundary types are allowed when required by Varjo Native SDK APIs, and that D3DHelper may coexist in applications or samples without becoming a VarjoToolkit dependency.
- Removed the core Boost dependency by replacing the eye tracking frame history `boost::circular_buffer` usage with `std::deque`-based bounded history logic.
- Removed Boost discovery / FetchContent fallback from CMake.

### Known limitations

- D3D rendering samples are minimal clear/triangle samples, not full scene or texture upload examples.
- Video post process samples demonstrate constant-buffer-only effects. Additional shader texture input samples are not included yet.
- Occlusion mesh is dumped to CSV/OBJ, but GPU buffer / stencil integration is not included yet.
- Several APIs require a connected Varjo HMD, Varjo Base, and corresponding runtime capabilities for full validation.

## v0.1.0 - Initial wrapper release

This is the first tagged release of VarjoToolkit.

### Added

- Core RAII wrappers
  - `VarjoSession`
  - `VarjoFrameInfo`
  - `VarjoScopedLock`
  - `VarjoEventQueue`
- DataStream wrappers
  - `VarjoDataStream`
  - `VarjoDataStreamBufferLock`
  - `VarjoDataStreamFrameQueue`
- MR wrappers
  - `VarjoCameraProperties`
  - `VarjoChromaKey`
- World / marker wrappers
  - `VarjoWorld`
  - `VarjoMarkerTracker`
- Rendering wrappers
  - `VarjoOcclusionMesh`
  - `VarjoSwapChain`
  - `VarjoMultiProjLayer`
  - `VarjoLayerFrame`
- Utility APIs
  - `VarjoTimestampMapping`
  - `VarjoToolkit::Csv`
  - `VarjoToolkit/Version.hpp`
- Service loggers
  - `VarjoEyeTrackingService`
  - `VarjoIMUService`
  - `VarjoVSTService`
  - `VarjoEyeCameraService`
  - `VarjoEnvironmentCubemapService`
- `ServiceLoggerSample`
- CMake-based Boost header fetch fallback
- HMD-independent unit/smoke tests for wrapper helper behavior, null-session failure paths, queue behavior, timestamp formatting, CSV conversion, and layer/world structure helpers.

### Known limitations

- `VarjoSwapChain` / `VarjoLayerFrame` are low-level wrappers only. A full D3D11/D3D12 rendering sample is not included in v0.1.0.
- `VarjoEventQueue` is implemented, but a dedicated event logger service is not included yet.
- `VarjoMarkerTracker` is implemented, but a dedicated marker tracking CSV service is not included yet.
- `VarjoOcclusionMesh` is implemented, but GPU buffer conversion / rendering pipeline integration is not included yet.
- Several APIs require a connected Varjo HMD, Varjo Base, and corresponding runtime capabilities for full validation.
- Coverage-oriented tests are included, but an automated coverage-report target is not included yet.

### Recommended validation before tagging

```bat
git pull

cmake --build out\build\test

ctest --test-dir out\build\test --output-on-failure
```
