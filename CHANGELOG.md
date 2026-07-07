# Changelog

## Unreleased

### Changed

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
