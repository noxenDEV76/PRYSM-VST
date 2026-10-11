<#
 Full local Windows build: configure, build (Release), test, create the installer.
 Prerequisites: Visual Studio 2022 (Desktop C++ workload), CMake >= 3.22, NSIS 3, Python 3, Git.
 Usage:  .\scripts\build-windows.ps1
#>
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
$juceCommit = "91ad83ae34a81e0833b1a2b0866f54846370ae53"
if (-not (Test-Path "third_party\JUCE\CMakeLists.txt")) {
    git clone --branch 8.0.15 --depth 1 https://github.com/juce-framework/JUCE.git third_party/JUCE
}
$sha = (git -C third_party/JUCE rev-parse HEAD).Trim()
if ($sha -ne $juceCommit) { throw "Unexpected JUCE commit $sha (expected $juceCommit)" }

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DPRYSM_BUILD_TESTS=ON
if ($LASTEXITCODE) { throw "configure failed" }
cmake --build build --config Release --parallel
if ($LASTEXITCODE) { throw "build failed" }
$dll = "build\PRYSM_artefacts\Release\VST3\PRYSM.vst3\Contents\x86_64-win\PRYSM.vst3"
if (-not (Test-Path $dll)) { throw "VST3 binary not found: $dll" }
ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE) { throw "tests failed" }
& "$PSScriptRoot\build-installer.ps1" -Vst3Dir "build\PRYSM_artefacts\Release\VST3\PRYSM.vst3"
