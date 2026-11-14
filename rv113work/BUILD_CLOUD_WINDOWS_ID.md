# Build REALVISION RENDER di cloud (tanpa CMake/Visual Studio di laptop)

Cara ini memakai GitHub Actions pada server Windows. Laptop hanya dipakai untuk mengunggah source dan mengunduh ZIP hasil build. Build yang disediakan adalah **CPU reference / CPU-only**; ini bukan renderer RTX/GPU.

## Langkah
1. Masuk ke https://github.com dan buat akun jika belum punya.
2. Klik tombol **New repository**. Beri nama `realvision-render`, pilih **Private**, lalu **Create repository**.
3. Pada halaman repository, pilih **Add file > Upload files**.
4. Ekstrak source ZIP REALVISION RENDER di laptop. Unggah seluruh isi folder hasil ekstrak, termasuk folder tersembunyi `.github` dan file workflow `.github/workflows/build-windows.yml`. Jika browser tidak menampilkan folder `.github`, aktifkan tampilan hidden items atau gunakan GitHub web upload dengan memastikan struktur folder workflow ikut dibuat. Struktur wajib: `.github/workflows/build-windows.yml`.
5. Klik **Commit changes**.
6. Buka tab **Actions**, pilih **Build REALVISION RENDER (Windows x64)**, klik **Run workflow** bila workflow belum berjalan otomatis.
7. Tunggu sampai job selesai dengan tanda hijau. Buka hasil run, bagian **Artifacts**, unduh `REALVISION_RENDER_1_14_WINDOWS_PORTABLE`.
8. Ekstrak ZIP. Di dalamnya ada `realvision_render.exe`, `sample.rvscene`, dan `PAKAI.txt`.
9. Di SketchUp, buka Extensions > REALVISION RENDER 1.14 > pilih executable, lalu arahkan ke `realvision_render.exe` hasil ekstrak.

## Batasan
- Workflow memakai runner Windows x64 dan mematikan Vulkan agar tidak memerlukan Vulkan SDK.
- Hasilnya executable Windows CPU reference renderer. Pengujian otomatis hanya smoke test CLI dan render BMP kecil.
- Workflow tidak membuktikan plugin berjalan di SketchUp Windows milik pengguna.
- RTX 4050 GPU rendering belum aktif; pipeline Vulkan ray tracing belum selesai.
- Jika unggah folder `.github` lewat browser terlalu merepotkan, minta bantuan untuk membuat struktur repository; jangan memasang Visual Studio/CMake lokal.
