<#
build.ps1 - PowerShell build script
Usage: .\build.ps1 [-Clean] [-Jobs N]
#>

param(
    [switch]$Clean,
    [int]$Jobs = 0
)

$CMAKE_BUILD_DIR = "build"
$OUT_DIR = "out"
$RESOURCES_DIR = "src\main\resources"
$LIB_REL_PATH = "windows\cjyaml.dll"
$BUILD_TYPE = "Release"

function Info($m){ Write-Host "[INFO] $m" -ForegroundColor Cyan }
function ErrorExit($m,$code=1){ Write-Host "[ERROR] $m" -ForegroundColor Red; exit $code }
function Success($m){ Write-Host "[SUCCESS] $m" -ForegroundColor Green }

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { ErrorExit "cmake not found. Install CMake." 2 }
if (-not (Get-Command mvn -ErrorAction SilentlyContinue))   { ErrorExit "mvn not found. Install Maven." 2 }

if ($Jobs -le 0) {
    try { $Jobs = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors } catch { $Jobs = 1 }
}

Info "Jobs = $Jobs, Clean = $Clean"

if ($Clean.IsPresent) {
    Info "Removing $CMAKE_BUILD_DIR and $OUT_DIR"
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $CMAKE_BUILD_DIR, $OUT_DIR
}

New-Item -ItemType Directory -Force -Path $CMAKE_BUILD_DIR | Out-Null
New-Item -ItemType Directory -Force -Path $RESOURCES_DIR | Out-Null

Info "Configuring CMake..."
cmake -S . -B $CMAKE_BUILD_DIR -DENABLE_JNI=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE
if ($LASTEXITCODE -ne 0) { ErrorExit "CMake configure failed." $LASTEXITCODE }

Info "Building..."
cmake --build $CMAKE_BUILD_DIR --config $BUILD_TYPE -- /m:$Jobs
if ($LASTEXITCODE -ne 0) { ErrorExit "Build failed." $LASTEXITCODE }

$libPath = Join-Path $OUT_DIR $LIB_REL_PATH
if (-not (Test-Path $libPath)) {
    $found = Get-ChildItem -Path $OUT_DIR,$CMAKE_BUILD_DIR -Filter "cjyaml.*" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $libPath = $found.FullName; Info "Found library: $libPath" } else { ErrorExit "Artifact not found: $libPath" 3 }
}

Info "Copying $libPath -> $RESOURCES_DIR"
Copy-Item -Force $libPath $RESOURCES_DIR

Info "Running Maven package..."
mvn -B clean package
if ($LASTEXITCODE -ne 0) { ErrorExit "Maven build failed." $LASTEXITCODE }

Success "Build complete. Library copied to $RESOURCES_DIR"
