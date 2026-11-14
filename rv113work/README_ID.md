# REALVISION RENDER 1.10 — Windows readiness & Vulkan diagnostics

Versi 1.10 memprioritaskan jalur uji Windows x64 dan diagnostik Vulkan/RTX sebelum mengklaim GPU rendering. Engine render produksi pada versi ini masih CPU reference path tracer dengan BVH. Tombol `PERIKSA VULKAN / RTX 4050` menjalankan `--gpu-info` dan `--rt-init` untuk memeriksa driver/loader, ekstensi Vulkan RT, fitur perangkat, dan pembuatan logical device.

## Build Windows paling mudah

1. Install Visual Studio 2022 (workload **Desktop development with C++**) dan CMake 3.24+.
2. Untuk pemeriksaan RTX, install **NVIDIA Studio/Game Ready Driver** terbaru dan **LunarG Vulkan SDK**. Pastikan installer mengatur variabel `VULKAN_SDK`.
3. Ekstrak source ZIP ke folder sederhana, misalnya `C:\REALVISION_RENDER_110`.
4. Buka **Developer PowerShell for VS 2022** (x64), masuk ke folder tersebut, lalu jalankan:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\build_windows.ps1
```

Executable dibuat di `build\Release\realvision_render.exe`. Jika Vulkan SDK tidak terdeteksi, script tetap membangun CPU-only dan memberi peringatan.

## Pasang di SketchUp 2025

1. SketchUp > Extension Manager > Install Extension, pilih RBZ.
2. Restart SketchUp jika diperlukan.
3. Extensions > REALVISION RENDER 1.10.
4. Pilih `realvision_render.exe` hasil build Windows.
5. Klik **PERIKSA VULKAN / RTX 4050** untuk diagnostik.
6. Tombol render preview masih menjalankan CPU reference renderer.

## Status yang benar

| Kemampuan | Status |
|---|---|
| Windows x64 build script | Disediakan; belum dieksekusi pada Windows di lingkungan pengembangan ini |
| SketchUp 2025 extension package | RBZ dikemas; runtime Windows belum diuji |
| Vulkan loader/device/RT feature probe | Kode diagnostik; perlu diuji pada PC Windows pengguna |
| Vulkan RT logical device initialization | Percobaan inisialisasi; belum berarti render GPU |
| `vkCmdTraceRaysKHR` pipeline + SBT | Belum diimplementasikan |
| RTX 4050 GPU rendering | Belum aktif |
| CPU reference renderer + BVH | Aktif; bukan real-time |

**Penting:** RTX 4050 mendukung hardware ray tracing, tetapi REALVISION RENDER belum mengirim ray tracing workload ke GPU. `--rt-init` hanya menguji pembuatan logical device Vulkan. Tidak ada klaim bahwa versi ini sudah merender menggunakan RTX 4050. Render CPU dari callback SketchUp juga masih sinkron dan dapat membuat UI menunggu.

## Format tekstur

Loader PPM P3/P6 dan BMP tersedia tanpa codec eksternal. PNG/JPEG diaktifkan hanya jika libpng/libjpeg ditemukan saat build; script Windows standar belum otomatis mengambil dependensi tersebut. UV dan bilinear sampling tersedia untuk base-color texture.


## Versi 1.11 - diagnostik Vulkan RT yang lebih rinci
- Menampilkan daftar physical device, vendor, versi Vulkan, dan dukungan ekstensi AS/RT.
- Memeriksa fitur accelerationStructure, rayTracingPipeline, dan bufferDeviceAddress.
- Membuat logical device pada GPU yang memenuhi syarat dan melaporkan properti ray tracing.
- BELUM membuat ray-tracing pipeline, BLAS/TLAS, shader binding table, atau menjalankan vkCmdTraceRaysKHR.
- Pengujian Windows/RTX 4050 harus dilakukan pada PC target; build Linux tanpa Vulkan SDK hanya menguji CPU.


## Versi 1.12 - alur build Windows dan BLAS/TLAS
- Perintah --build-as sekarang mencetak nama perangkat dan jumlah segitiga selain status build.
- Memperbaiki pemisah baris pada keluaran diagnostik RT.
- Memperbarui build_windows.ps1 agar melakukan build Release x64, menemukan executable, lalu menjalankan --gpu-info.
- Kode BLAS/TLAS Vulkan sebelumnya dipertahankan; fitur ini hanya dapat dijalankan bila build memakai Vulkan SDK dan perangkat RT yang sesuai.
- Pipeline ray tracing, shader binding table, dispatch vkCmdTraceRaysKHR, dan output gambar GPU belum tersedia.


## Versi 1.13 - alur kompilasi shader Windows
- Menambahkan compile_shaders_windows.ps1 untuk mengompilasi shader GLSL ke SPIR-V menggunakan glslc.
- Build Windows mencoba kompilasi shader secara opsional jika Vulkan SDK tersedia.
- Shader masih prototipe dan belum terhubung ke VkRayTracingPipeline.
- Descriptor sets, SBT, command buffer dispatch, dan vkCmdTraceRaysKHR belum tersedia.


## Versi 1.14 - kompatibilitas integrasi Windows
- Perintah render dari Ruby kini memakai system(executable, *argv), bukan command string yang menggabungkan path; ini lebih aman untuk path Windows yang mengandung spasi.
- Lokasi executable disimpan melalui Sketchup.write_default dan dipulihkan otomatis pada render/diagnostik.
- Folder output sementara dan nama preview dibedakan untuk versi 1.14.
- Ini memperbaiki jalur integrasi secara statis; runtime SketchUp Windows tetap harus diuji di SketchUp Windows asli.
