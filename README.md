# lotus-engine
This repository is the implementation of lotus-engine, plus an implementation of FFXI using it.

Currently it can only load maps, non-player character models, animations, some schedulers/generators/particles, and collision meshes.
It is mostly just a demonstration of lotus-engine as I implement various rendering techniques for fun.  

# Build Requirements
* Visual Studio 2019 16.9 (as of writing, this requires VS 2019 Preview)
* Linux build was supported, but is currently waiting on GCC/Clang to update their C++20 support
* Vulkan SDK with VK_KHR_raytracing (1.2.162.0 or higher)
* SDL2 (included in Vulkan SDK on Windows)
* glm (included in Vulkan SDK on Windows)

# Running Requirements
* GPU compatible with VK_KHR_raytracing (any RTX card or GTX1000 card probably, plus up to date drivers)
* FFXI installed somewhere (located via registry)
