REALVISION RENDER 1.13 - Vulkan RT shader prototypes

raygen.rgen: ray generation stage, primary rays and RGBA32F output.
miss.rmiss: background payload.
closesthit.rchit: initial hit-shading prototype.

Windows compile:
1. Install LunarG Vulkan SDK (includes glslc).
2. Run compile_shaders_windows.ps1.
3. SPIR-V output: build-windows/shaders/*.spv

IMPORTANT: these shaders are not yet connected to a VkRayTracingPipeline.
Runtime does not yet create descriptor sets, shader modules/groups, SBT,
ray-tracing command buffers, or dispatch vkCmdTraceRaysKHR. closest-hit uses
a placeholder normal derived from ray direction. No GPU image is produced.
