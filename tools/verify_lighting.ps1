param([string]$Executable=(Join-Path $PSScriptRoot '../build-msvc-ninja/MiniCity3D.exe'),
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../evidence/lighting-20260928'))
$ErrorActionPreference='Stop'
$executablePath=(Resolve-Path -LiteralPath $Executable).Path
$runtimeDirectory=Split-Path -Parent $executablePath
$outputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$log=Join-Path $runtimeDirectory 'MiniCity3D.log'
$scenes=@(
    @{Name='sun';Arguments='--sun-preview'},
    @{Name='flare-visible';Arguments='--sun-preview --flare-view'},
    @{Name='flare-disabled';Arguments='--sun-preview --flare-view --no-lens-flare'},
    @{Name='flare-occluded';Arguments='--sun-preview --sun-occluded --flare-view'},
    @{Name='flare-night';Arguments='--sun-preview --night --flare-view'},
    @{Name='facade';Arguments='--reflection-preview --high-shadows'},
    @{Name='facade-no-sun';Arguments='--reflection-preview --high-shadows --no-direct-sun'},
    @{Name='car-coat';Arguments='--golden-hour --driver-preview --modern-car-preview'},
    @{Name='car-no-coat';Arguments='--golden-hour --driver-preview --modern-car-preview --no-clearcoat'},
    @{Name='night';Arguments='--night --modern-street-preview --high-shadows'},
    @{Name='rain';Arguments='--day --rain --modern-street-preview --high-taa'}
)
$results=@()
foreach($scene in $scenes){
    $before=if(Test-Path -LiteralPath $log){(Get-Content -LiteralPath $log).Count}else{0}
    $arguments="--smoke --1080p --screenshot $($scene.Arguments)"
    $process=Start-Process -FilePath $executablePath -WorkingDirectory $runtimeDirectory `
        -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(-not $process.WaitForExit(120000)){
        Stop-Process -Id $process.Id
        throw "Rendering timed out: $($scene.Name)"
    }
    if($process.ExitCode -ne 0){throw "Rendering failed: $($scene.Name), $($process.ExitCode)"}
    $lines=@(Get-Content -LiteralPath $log | Select-Object -Skip $before)
    $lines | Set-Content -LiteralPath (Join-Path $outputDirectory "$($scene.Name).txt")
    $capture=@($lines | Select-String 'Screenshot saved: screenshots/(.+\.png)')
    if($capture.Count -ne 1){throw "Missing capture: $($scene.Name)"}
    $destination=Join-Path $outputDirectory "$($scene.Name).png"
    Copy-Item -LiteralPath (Join-Path $runtimeDirectory "screenshots/$($capture[0].Matches[0].Groups[1].Value)") -Destination $destination -Force
    $results+=@{Name=$scene.Name;Arguments=$arguments;ExitCode=$process.ExitCode;
        ScreenshotSha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash}
    Write-Host "Verified $($scene.Name)"
}
@{ExecutableSha256=(Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash;Scenes=$results} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputDirectory 'captures.json')
