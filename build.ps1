param([string]$Toolchain = 'D:\Dev\mingw64\bin')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    # This toolchain appends its own manifest; use the application's manifest only.
    $specs = (& "$Toolchain\g++.exe" -dumpspecs) -join "`n"
    $specs.Replace('%{!shared:%:if-exists(default-manifest.o%s)}', '') | Set-Content -LiteralPath 'compiler.specs' -Encoding ascii
    & "$Toolchain\windres.exe" --codepage=65001 resources.rc -O coff -o resources.o
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed' }
    & "$Toolchain\g++.exe" '-specs=compiler.specs' pet.cpp resources.o -o 'pet-native.exe' -std=c++17 -O2 -s -static -municode -mwindows -finput-charset=UTF-8 -Wall -Wextra -Wno-missing-field-initializers -lgdiplus -lshlwapi -lole32 -lwinmm -lgdi32 -luser32
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    Move-Item -LiteralPath 'pet-native.exe' -Destination '..\奶螈桌宠 1.1.0-原生版.exe' -Force
} finally { Pop-Location }
