# lotus-engine
This repository is the implementation of lotus-engine, plus an implementation of FFXI using it.

Currently it can only load maps, non-player character models, animations, some schedulers/generators/particles, and collision meshes.
It is mostly just a demonstration of lotus-engine as I implement various rendering techniques for fun.  

# Build Requirements
* Visual Studio 2019 (Windows) or recent CMake and GCC8+ (Linux)
* Vulkan SDK supporting 1.2 or higher
* SDL2 (included in Vulkan SDK on Windows)
* glm (included in Vulkan SDK on Windows)

# Running Requirements
* GPU compatible with VK_KHR_raytracing (this also implies Vulkan 1.2 support for drivers)
* FFXI installed somewhere (located via registry)
