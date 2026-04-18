# VoxelForge

An original infinite-terrain voxel game built from scratch.

## Targets
- Android APK (arm64-v8a, armeabi-v7a)
- Web (WebAssembly via Emscripten)

## Building

### Android APK
```bash
chmod +x ./build.sh
./build.sh
```

### Web (Emscripten)
```bash
emcmake cmake -B build -S . -G Ninja
cmake --build build --target VoxelForge --parallel
```

## License
Original engine code © respective authors. VoxelForge modifications © GAX Studios.
