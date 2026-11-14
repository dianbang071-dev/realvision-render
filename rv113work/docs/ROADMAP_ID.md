# REALVISION RENDER — Roadmap

## 1.10 — Windows readiness & Vulkan diagnostics
- PowerShell build script untuk Visual Studio 2022 x64.
- Deteksi LunarG Vulkan SDK melalui VULKAN_SDK.
- Pemeriksaan Vulkan RT dan logical-device init dari panel SketchUp.
- Tidak mengklaim GPU rendering sebelum trace dispatch aktif.

## Tahap GPU selanjutnya
- Implementasi BLAS/TLAS dan descriptor buffers yang benar-benar dipakai pipeline.
- Ray tracing pipeline, shader groups, shader binding table, `vkCmdTraceRaysKHR`.
- Output image GPU, progressive accumulation, dan denoising.
- Uji runtime pada Windows + SketchUp 2025 + RTX 4050.


## 1.12
Fokus: Windows build x64, diagnosa GPU, dan keluaran detail BLAS/TLAS.
Status: implementasi BLAS/TLAS sudah ada pada jalur Vulkan, tetapi perlu diuji di Windows dengan Vulkan SDK + GPU RT.
Belum tersedia: ray tracing pipeline lengkap, SBT, vkCmdTraceRaysKHR, dan framebuffer GPU.


## 1.13
Fokus: alur kompilasi shader Windows (glslc -> SPIR-V).
Status: helper PowerShell tersedia; lingkungan ini tidak memiliki glslc sehingga SPIR-V belum dapat diuji.
Belum tersedia: runtime VkRayTracingPipeline, descriptor sets, SBT, dan vkCmdTraceRaysKHR.
