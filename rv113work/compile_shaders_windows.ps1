$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Glslc = Get-Command glslc -ErrorAction SilentlyContinue
if (-not $Glslc) { throw "glslc tidak ditemukan. Install LunarG Vulkan SDK dan pastikan Bin masuk PATH." }
$Out = Join-Path $Root "build-windows\shaders"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Shaders = @("raygen.rgen","miss.rmiss","closesthit.rchit")
foreach ($Shader in $Shaders) {
  $Input = Join-Path $Root "engine\shaders\$Shader"
  $Output = Join-Path $Out ($Shader + ".spv")
  & $Glslc.Source --target-env=vulkan1.2 $Input -o $Output
  if ($LASTEXITCODE -ne 0) { throw "Kompilasi shader gagal: $Shader" }
  Write-Host "OK: $Output" -ForegroundColor Green
}
Write-Host "Shader SPIR-V selesai dibuat. Pipeline GPU runtime masih tahap berikutnya."
