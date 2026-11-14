$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root "build-windows"
Write-Host "REALVISION RENDER 1.12 - Windows x64 build"
Write-Host "Checking CMake..."
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  throw "CMake tidak ditemukan. Install CMake 3.24+ dan Visual Studio 2022 C++ workload."
}
cmake -S $Root -B $Build -A x64 -DRV_ENABLE_VULKAN=ON -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configure gagal." }
cmake --build $Build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build gagal." }
$Exe = Join-Path $Build "Release\realvision_render.exe"
if (-not (Test-Path $Exe)) { $Exe = Join-Path $Build "realvision_render.exe" }
if (-not (Test-Path $Exe)) { throw "Executable tidak ditemukan setelah build." }
Write-Host ""
Write-Host "BUILD BERHASIL: $Exe" -ForegroundColor Green
Write-Host "Langkah pemeriksaan GPU:"
& $Exe --gpu-info
Write-Host ""
Write-Host "Untuk membuat BLAS/TLAS dari scene contoh:"
Write-Host "`"$Exe`" --build-as `"$Root\sample.rvscene`""


# Compile RT shader prototypes when Vulkan SDK glslc is available.
if (Get-Command glslc -ErrorAction SilentlyContinue) {
  Write-Host "Compiling Vulkan RT shader prototypes..."
  & (Join-Path $Root "compile_shaders_windows.ps1")
} else {
  Write-Warning "glslc tidak ditemukan; kompilasi shader dilewati."
}
