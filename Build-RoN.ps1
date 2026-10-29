$ErrorActionPreference = 'Stop'
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskMSBuild = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1
if (!$taskMSBuild) { throw 'Visual Studio 2022 C++ Build Tools bulunamadi.' }
& $taskMSBuild (Join-Path $PSScriptRoot 'RoN-Internal.sln') /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw 'Derleme basarisiz.' }
Write-Host 'Hazir: x64\Release\SCC-ReadyOrNot.dll'
