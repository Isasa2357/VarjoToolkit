# Changelog

## Unreleased

### Added

- `VarjoOcclusionMeshDumpSample`.
  - Dumps Varjo occlusion meshes for all runtime views.
  - Writes vertex CSV and OBJ triangle-list files.
  - Supports clockwise and counter-clockwise winding selection.
- `VarjoVideoPostProcessD3D12Sample`.
  - Demonstrates `VarjoVideoPostProcessShader` with D3D12.
  - Creates a native D3D12 command queue and passes it to `configureD3D12`.
  - Compiles an embedded HLSL compute shader in the sample and passes the compiled blob to the wrapper.
  - Submits a trivially-copyable constant buffer struct without the wrapper interpreting its fields.
  - Applies a simple circular VST dimming/highlight effect and updates parameters at runtime.
- `VarjoVideoPostProcessD3D11Sample`.
  - Demonstrates `VarjoVideoPostProcessShader` with D3D11.
  - Compiles an embedded HLSL compute shader in the sample and passes the compiled blob to the wrapper.
  - Submits a trivially-copyable constant buffer struct without the wrapper interpreting its fields.
  - Applies a simple circular VST dimming/highlight effect and updates parameters at runtime.
- CMake install / package support.
  - Installs the `VarjoToolkit` static library and public headers.
  - Exports `VarjoToolkit::VarjoToolkit` through `VarjoToolkitTargets.cmake`.
  - Generates `VarjoToolkitConfig.cmake` and `VarjoToolkitConfigVersion.cmake` for `find_package(VarjoToolkit CONFIG REQUIRED)`.
- `scripts/run_coverage_open_cpp_coverage.bat`.
  - Builds Debug tests with Visual Studio generator.
  - Runs HMD-independent tests through OpenCppCoverage.
  - Exports HTML and Cobertura XML reports under `out/coverage`.
- `VarjoVideoPostProcessShader`
  - Thin wrapper for Varjo Native SDK experimental Video Post Process Shader API.
  - Accepts compiled shader bytecode as `const void* + size`; it does not compile HLSL source.
  - Accepts constant buffer updates as raw bytes and provides trivially-copyable struct template helpers.
  - Holds `varjo_LockType_VideoPostProcessShader` through RAII.
  - Supports D3D11 and D3D12 configure entry points with native DirectX objects supplied by the caller.
  - Provides supported texture format query helpers.
- `VarjoShaderTextureLock`
  - RAII wrapper for `varjo_MRAcquireShaderTexture` / `varjo_MRReleaseShaderTexture`.
- `VarjoToolkitVideoPostProcessShaderTest`
  - HMD-independent tests for bytecode views, config helpers, texture config helpers, null-session failure paths, and template constant buffer helpers.
- `VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS` CMake option.
  - Enabled by default.
  - Requires `include_experimental` from Varjo experimental SDK.
- `VarjoEventService`
  - Polls `VarjoEventQueue` in a worker thread.
  - Writes event rows to CSV with row index, readable event type name, and `varjo_Event` payload fields.
  - Provides a bounded in-memory queue for application-side event consumption.
- `VarjoMarkerTrackingService`
  - Samples `VarjoMarkerTracker` in a worker thread.
  - Writes marker object/component/pose rows to CSV.
  - Provides a bounded in-memory queue for application-side marker consumption.
- `VarjoToolkitEventMarkerServicesTest`
  - HMD-independent tests for event/marker CSV row formatting, file output, and null-session service failure paths.
- `ServiceLoggerSample` integration for event and marker logging.
  - Added `--no-event` and `--no-marker` options.

### Changed

- Public include paths now use build/install generator expressions so the target can be exported cleanly.
- Documented install/package and coverage workflows in `README.md`.
- Documented the Varjo Native SDK video post process shader wrapper policy in `docs/ARCHITECTURE.md`.
- Removed the core Boost dependency by replacing the eye tracking frame history `boost::circular_buffer` usage with `std::deque`-based bounded history logic.
- Removed Boost discovery / FetchContent fallback from CMake.
- Documented the VarjoToolkit dependency and architecture policy in `docs/ARCHITECTURE.md`.
- Clarified that VarjoToolkit core must not depend on D3DHelper / D3D11Helper / D3D12Helper, OpenCV, JSON libraries, camera SDK wrappers, UI frameworks, or ML frameworks.
- Clarified that native DirectX SDK boundary types are allowed when required by Varjo Native SDK APIs, and that D3DHelper may coexist in applications or samples without becoming a VarjoToolkit dependency.

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
