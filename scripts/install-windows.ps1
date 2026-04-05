#Requires -RunAsAdministrator
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Info($msg)  { Write-Host "[info]  $msg" -ForegroundColor Blue }
function Warn($msg)  { Write-Host "[warn]  $msg" -ForegroundColor Yellow }
function Error($msg) { Write-Host "[error] $msg" -ForegroundColor Red; exit 1 }

Info "Demiplane — Windows install script"
Write-Host ""

# ── winget packages ──────────────────────────────────────────────────────────

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Error "winget is required but not found. Install it from: https://aka.ms/getwinget"
}

Info "Installing packages via winget..."

$packages = @(
    "Kitware.CMake"
    "Ninja-build.Ninja"
    "LLVM.LLVM"
    "Git.Git"
    "GnuWin32.Bison"
    "GnuWin32.Flex"
    "Ccache.Ccache"
)

foreach ($pkg in $packages) {
    Info "  Installing $pkg..."
    winget install --id $pkg --accept-source-agreements --accept-package-agreements --silent 2>$null
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) {
        # -1978335189 = already installed
        Warn "  Failed to install $pkg (exit code: $LASTEXITCODE)"
    }
}

# ── Visual Studio Build Tools ────────────────────────────────────────────────

Info "Checking for Visual Studio Build Tools..."

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsInstall = & $vsWhere -latest -property installationPath 2>$null
    if ($vsInstall) {
        Info "Visual Studio found at: $vsInstall"
    } else {
        Warn "Visual Studio installer found but no installation detected."
        Warn "Install Build Tools from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022"
        Warn "Select 'Desktop development with C++' workload."
    }
} else {
    Warn "Visual Studio Build Tools not found."
    Warn "Install from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022"
    Warn "Select 'Desktop development with C++' workload."
}

# ── vcpkg ────────────────────────────────────────────────────────────────────

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$vcpkgDir = Join-Path $repoRoot "vcpkg"

if (Test-Path $vcpkgDir) {
    Info "vcpkg already exists at $vcpkgDir — skipping"
} else {
    Info "Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg $vcpkgDir

    Info "Bootstrapping vcpkg..."
    $env:VCPKG_DISABLE_METRICS = "1"
    & "$vcpkgDir\bootstrap-vcpkg.bat"
}

Write-Host ""
Info "Installation complete. Build with:"
Write-Host ""
Write-Host "  cmake --preset=debug"
Write-Host "  cmake --build build/debug"
Write-Host ""
