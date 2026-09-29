param([string]$Executable=(Join-Path $PSScriptRoot '../build-msvc-ninja/MiniCity3D.exe'),
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../evidence/modern-assets-20260928'),
    [string]$ExtraArguments='')
$ErrorActionPreference='Stop'
$executablePath=(Resolve-Path -LiteralPath $Executable).Path
$runtimeDirectory=Split-Path -Parent $executablePath
$outputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$log=Join-Path $runtimeDirectory 'MiniCity3D.log'
$scenes=@(
    @{Name='dx11-day';Arguments='--day'},
    @{Name='dx11-street';Arguments='--day --modern-street-preview'},
    @{Name='dx11-night';Arguments='--night --modern-street-preview --high-shadows'},
    @{Name='dx11-car';Arguments='--day --driver-preview --modern-car-preview'},
    @{Name='dx11-coupe';Arguments='--day --driver-preview --modern-coupe-preview'},
    @{Name='dx11-marina';Arguments='--night --marina'},
    @{Name='dx11-pistol';Arguments='--day --weapon-preview'},
    @{Name='dx11-carbine';Arguments='--day --weapon-preview --rifle-preview'}
)
$results=@()
foreach($scene in $scenes){
    $before=if(Test-Path -LiteralPath $log){(Get-Content -LiteralPath $log).Count}else{0}
    $arguments="--smoke --1080p --screenshot $($scene.Arguments) $ExtraArguments"
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
