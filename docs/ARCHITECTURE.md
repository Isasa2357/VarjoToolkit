# VarjoToolkit Architecture Policy

## Role

VarjoToolkit is a thin C++ wrapper layer around Varjo Native SDK.

The library is intended to provide a stable entry point for Varjo sessions, frame information, data streams, mixed reality controls, world/marker tracking, video post process shader configuration, and layer/swapchain submission helpers. Application-specific rendering, camera, computer vision, machine learning, or UI pipelines should live outside this library.

## Dependency policy

The core library should depend only on:

- C++ standard library
- Varjo Native SDK
- Windows / DirectX SDK types when required by the Varjo Native SDK boundary
- General-purpose low-level libraries only when there is clear value

Examples of acceptable optional/general-purpose dependencies, if a future need arises:

- Boost, when it replaces missing standard-library functionality and is worth the dependency cost
- GLM, when math types/functions are needed and direct Varjo math types are insufficient
- nlohmann/json only in samples or tools, not in the core library, unless a strong core-library need appears

The v0.1.x core library intentionally has no Boost dependency after replacing the previous circular buffer usage with standard-library containers.

## Disallowed core dependencies

The core library must not depend on high-level or domain-specific libraries such as:

- OpenCV
- camera SDK abstraction libraries
- application-specific rendering frameworks
- D3DHelper / D3D11Helper / D3D12Helper
- ML / inference libraries
- UI frameworks

These dependencies may be used by applications or samples when appropriate, but they should not become required by VarjoToolkit itself.

## DirectX policy

DirectX itself is allowed at the Varjo Native SDK boundary.

For example, `VarjoSwapChain` may accept native Direct3D objects such as `ID3D11Device*` or `ID3D12CommandQueue*` because those are part of the Varjo swapchain creation API surface.

The experimental video post process shader wrapper may also accept native Direct3D objects because Varjo Native SDK exposes `varjo_MRD3D11ConfigureShader` and `varjo_MRD3D12ConfigureShader` directly.

However, VarjoToolkit should not own a high-level Direct3D backend. If minimal Direct3D code is required in the core library, write it directly against the Windows / DirectX SDK. Do not depend on D3DHelper.

Applications may use D3DHelper and VarjoToolkit together. VarjoToolkit should therefore accept native Direct3D resources and objects supplied by the caller rather than forcing a particular helper framework.

## Video post process shader policy

Varjo Native SDK's video post process shader API is allowed in VarjoToolkit because it is a Native SDK feature for VST / video pass-through processing, not an application-layer fullscreen effect.

The core wrapper should remain thin:

- Accept compiled shader bytecode as `const void* + size`.
- Do not compile HLSL source.
- Accept constant buffer data as raw bytes.
- Do not interpret user-defined constant buffer structs.
- Provide template helpers only to pass trivially copyable structs as bytes.
- Hold `varjo_LockType_VideoPostProcessShader` through RAII.
- Wrap shader input texture acquire/release through RAII.

The core wrapper must not contain effect-specific shaders such as blur, circular dimming, gaze highlight, color grading, segmentation compositing, or parameter UI. Those belong in applications, samples, or a future optional effects library.

## Sample policy

Samples may use additional dependencies when they demonstrate integration with real applications, but those dependencies should remain sample-local and optional.

Examples:

- A D3D11 or D3D12 sample may use D3DHelper if the sample is explicitly demonstrating integration, but the core library must not require it.
- A JSON configuration file may be parsed in a sample, but JSON should not become a core dependency unless necessary.
- OpenCV-based image processing should be outside VarjoToolkit core. If demonstrated, it should be in a separate sample/application layer.
- Video post process samples may include HLSL files and shader compilation steps, but the core library should accept only compiled shader bytecode.

## Design guideline

Prefer APIs that accept native resources from the caller:

- `varjo_Session*` or `std::shared_ptr<varjo_Session>`
- native Direct3D device/queue/resource pointers
- Varjo Native SDK structs
- compiled shader bytecode views
- raw byte views for constant buffers

Avoid APIs that own or hide large application subsystems such as a complete renderer, camera pipeline, image processing pipeline, shader build pipeline, or ML inference pipeline.
