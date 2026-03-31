param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$pythonExe = Join-Path $projectRoot "..\.venv\Scripts\python.exe"

if (-not (Test-Path $pythonExe)) {
    Write-Error "Python environment not found at $pythonExe. Configure the workspace Python environment first."
}

Push-Location $projectRoot
try {
    if ($Clean) {
        & $pythonExe -m platformio run -e rak_wismeshtag -t clean
    }

    & $pythonExe -m platformio run -e rak_wismeshtag
}
finally {
    Pop-Location
}
