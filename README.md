# lotus-engine
This repository is the implementation of lotus-engine, plus an implementation of FFXI using it.

Currently it can only load maps, non-player character models, animations, some schedulers/generators/particles, and collision meshes.
It is mostly just a demonstration of lotus-engine as I implement various rendering techniques for fun.  

# Build Requirements
* (Windows) Visual Studio 2022
* (Linux) GCC13
* Vulkan SDK 1.3.216 or higher
* SDL2 (included in Vulkan SDK on Windows)
* glm (included in Vulkan SDK on Windows)

# Build
    mkdir build
    cd build
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
    ninja

# Running Requirements
* GPU compatible with VK_KHR_raytracing (RTX2000+, AMD6000+)
* FFXI installed somewhere (located via registry (Windows) or environment variable FFXI_PATH (Linux))

# Run
    cd build/bin
    FFXI_PATH=(...) ./ffxi
