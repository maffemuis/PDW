param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "PDW executable not found: $Executable"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "vswhere.exe not found; cannot verify PE runtime imports"
}

$installationPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1).Trim()
if (-not $installationPath) {
    throw "Visual Studio VC tools installation not found"
}

$versionFile = Join-Path $installationPath "VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt"
if (-not (Test-Path -LiteralPath $versionFile -PathType Leaf)) {
    throw "VC tools version file not found: $versionFile"
}

$toolsVersion = (Get-Content -LiteralPath $versionFile -Raw).Trim()
$toolRoot = Join-Path $installationPath "VC\Tools\MSVC\$toolsVersion\bin"
$dumpbinCandidates = @(
    (Join-Path $toolRoot "Hostx64\x86\dumpbin.exe"),
    (Join-Path $toolRoot "Hostx86\x86\dumpbin.exe")
)
$dumpbin = $dumpbinCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $dumpbin) {
    throw "dumpbin.exe not found under VC tools $toolsVersion"
}

$imports = & $dumpbin /nologo /dependents $Executable 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin failed with exit code $LASTEXITCODE`n$($imports -join [Environment]::NewLine)"
}

$forbidden = @(
    '(?i)\bMSVCP\d+[^\s]*\.dll\b',
    '(?i)\bVCRUNTIME\d+[^\s]*\.dll\b',
    '(?i)\bucrtbase\.dll\b',
    '(?i)\bapi-ms-win-crt-[^\s]*\.dll\b'
)

$violations = foreach ($line in $imports) {
    foreach ($pattern in $forbidden) {
        if ($line -match $pattern) {
            $line.Trim()
            break
        }
    }
}

if ($violations) {
    throw "Portable PDW.exe unexpectedly depends on a dynamic C/C++ runtime:`n$($violations -join [Environment]::NewLine)"
}

Write-Host "Portable runtime dependency check passed."
