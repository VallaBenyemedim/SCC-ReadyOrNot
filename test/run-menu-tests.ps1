$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskMsbuild = & $taskVswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$taskMsbuild) { throw 'Visual Studio C++ Build Tools bulunamadi.' }
Push-Location $taskRoot
try {
    & $taskMsbuild test\MenuHarness.vcxproj /p:Configuration=Release /p:Platform=x64 /m /v:quiet /nologo *> build-menu-harness.log
    if ($LASTEXITCODE -ne 0) { Get-Content build-menu-harness.log -Tail 25; throw 'Menu test derlemesi basarisiz.' }
    $taskOutput = Join-Path $taskRoot 'artifacts\menu'
    New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
    $taskPages = @('overview','player','weapons','movement','npc','visuals','missions','colors','system','esp-style')
    for ($taskIndex=0; $taskIndex -lt $taskPages.Count; ++$taskIndex) {
        $taskPage=$taskPages[$taskIndex]
        $taskTestLog=Join-Path $taskOutput "$taskPage.log"
        & x64\MenuTests\MenuHarness.exe (Join-Path $taskOutput "$taskPage.png") $taskIndex *> $taskTestLog
        if ($LASTEXITCODE -ne 0 -or (Select-String -LiteralPath $taskTestLog -Pattern 'imgui-error|FAIL' -Quiet)) {
            Get-Content -LiteralPath $taskTestLog -Tail 10
            throw "$taskPage testi basarisiz."
        }
        Write-Output "$taskPage : $(Get-Content -LiteralPath $taskTestLog -Tail 1)"
    }
} finally { Pop-Location }
