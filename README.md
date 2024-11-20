# lotus-engine
This repository is the implementation of lotus-engine

While it does support rasterization, hybrid RT, and full RT, I mostly only test with full RT because it's the most fun.

For an implementation example, see https://github.com/teschnei/lotus-ffxi

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

