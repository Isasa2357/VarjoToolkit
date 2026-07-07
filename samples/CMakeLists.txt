add_subdirectory(ServiceLoggerSample)
add_subdirectory(D3D11RenderSample)
add_subdirectory(D3D12RenderSample)
add_subdirectory(OcclusionMeshDumpSample)

if (VARJOTOOLKIT_ENABLE_EXPERIMENTAL_MR_POSTPROCESS)
    add_subdirectory(VideoPostProcessD3D11Sample)
    add_subdirectory(VideoPostProcessD3D12Sample)
endif()
