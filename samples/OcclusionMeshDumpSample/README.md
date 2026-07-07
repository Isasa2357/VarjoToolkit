# VarjoOcclusionMeshDumpSample

Dumps Varjo occlusion meshes for all views.

The Varjo occlusion mesh is a 2D triangle list. This sample uses `VarjoOcclusionMesh`, snapshots the vertices, and writes them as CSV and OBJ files.

## Build

```bat
set "VARJO_SDK_ROOT=C:\work\library\varjo-sdk-experimental"
set "PATH=%VARJO_SDK_ROOT%\bin;%PATH%"

cmake -S . -B out\build\default ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DVARJO_SDK_ROOT="%VARJO_SDK_ROOT%" ^
  -DVARJOTOOLKIT_BUILD_SAMPLES=ON

cmake --build out\build\default --config Release
```

## Run

```bat
out\build\default\samples\OcclusionMeshDumpSample\Release\VarjoOcclusionMeshDumpSample.exe --out occlusion_dump
```

Options:

```bat
out\build\default\samples\OcclusionMeshDumpSample\Release\VarjoOcclusionMeshDumpSample.exe --out occlusion_dump --winding cw
out\build\default\samples\OcclusionMeshDumpSample\Release\VarjoOcclusionMeshDumpSample.exe --out occlusion_dump --winding ccw
out\build\default\samples\OcclusionMeshDumpSample\Release\VarjoOcclusionMeshDumpSample.exe --out occlusion_dump --no-obj
out\build\default\samples\OcclusionMeshDumpSample\Release\VarjoOcclusionMeshDumpSample.exe --out occlusion_dump --no-csv
```

Outputs:

```txt
summary.txt
view0_vertices.csv
view0_mesh.obj
view1_vertices.csv
view1_mesh.obj
...
```

This sample requires Varjo Base and a connected Varjo HMD.
