param(
    [string]$Runtime = 'build-msvc-ninja',
    [string]$Evidence = 'evidence/rendering-refactor-20261009',
    [string]$BaselineDirectory = '',
    [string[]]$SelectedConfigurations = @('dx11', 'dx12', 'dx12-fsr2', 'dx12-workers'),
    [string[]]$AdditionalArguments = @()
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$runtimePath = [IO.Path]::GetFullPath((Join-Path $repository $Runtime))
$evidencePath = [IO.Path]::GetFullPath((Join-Path $repository $Evidence))
$isolatedRoot = Join-Path $repository 'build-codex/rendering-refactor-runtime'
New-Item -ItemType Directory -Force -Path $evidencePath, $isolatedRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $repository 'assets') -Destination $isolatedRoot -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repository 'data') -Destination $isolatedRoot -Recurse -Force

$configurations = @(
    @{ Name = 'dx11'; Executable = 'MiniCity3D-DX11-benchmark.exe'; Backend = 11; Args = @() },
    @{ Name = 'dx12'; Executable = 'MiniCity3D.exe'; Backend = 12; Args = @('--fsr2=0') },
    @{ Name = 'dx12-fsr2'; Executable = 'MiniCity3D.exe'; Backend = 12; Args = @('--fsr2=1') },
    @{ Name = 'dx12-workers'; Executable = 'MiniCity3D.exe'; Backend = 12; Args = @('--fsr2=0', '--record-workers=4') }
)
$results = @()
foreach ($configuration in $configurations) {
    if ($configuration.Name -notin $SelectedConfigurations) { continue }
    $versions = @('candidate')
    if ($BaselineDirectory) { $versions = @('baseline', 'candidate') }
    foreach ($version in $versions) {
        $name = "$($configuration.Name)-$version"
        $directory = $isolatedRoot
        Copy-Item -LiteralPath (Join-Path $repository 'evidence/dx12-performance-20261009/settings.ini') -Destination (Join-Path $directory 'settings.ini') -Force
        $save = Join-Path $directory 'savegame.ini'
        if (Test-Path -LiteralPath $save) { Remove-Item -LiteralPath $save }
        $sourceExecutable = if ($version -eq 'baseline') {
            Join-Path (Join-Path $repository $BaselineDirectory) "baseline-dx$($configuration.Backend).exe"
        } else { Join-Path $runtimePath $configuration.Executable }
        $executable = Join-Path $directory 'MiniCity3D.exe'
        Copy-Item -LiteralPath $sourceExecutable -Destination $executable -Force
        $arguments = @('--smoke', '--benchmark', '--1080p', '--day', '--screenshot', '--validate-gpu-skinning') + $configuration.Args + $AdditionalArguments
        if ($configuration.Backend -eq 12) { $arguments += '--dx12-debug' }
        $started = Get-Date
        $process = Start-Process -FilePath $executable -WorkingDirectory $directory -ArgumentList $arguments -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(120000)) {
            Stop-Process -Id $process.Id
            throw "$name exceeded 120 seconds"
        }
        $process.Refresh()
        if ($process.ExitCode -ne 0) { throw "$name failed: $($process.ExitCode)" }
        $logPath = Join-Path $directory 'MiniCity3D.log'
        $log = [IO.File]::ReadAllText($logPath)
        $start = $log.LastIndexOf('Application started')
        if ($start -lt 0) { throw "$name missing application start" }
        $log = $log.Substring($start)
        [IO.File]::WriteAllText((Join-Path $evidencePath "$name.txt"), $log)
        if ($log -notmatch 'Smoke render completed' -or $log -match 'GPU skin validation: FAIL|using CPU deformation') {
            throw "$name did not complete with GPU skinning"
        }
        if (([regex]::Matches($log, 'GPU skin validation: PASS')).Count -ne 3) {
            throw "$name did not validate all three sampled poses"
        }
        if ($configuration.Backend -eq 12 -and $log -notmatch 'DX12 validation errors: 0') {
            throw "$name has no clean DX12 validation result"
        }
        if ('--validate-loading' -in $AdditionalArguments) {
            if ($log -match 'DDS GPU mip verification:[^\r\n]+FAILED' -or
                $log -notmatch 'DDS GPU mip verification:[^\r\n]+passed' -or
                $log -notmatch 'Texture checksum: Preparing regional textures') {
                throw "$name did not validate compressed uploads and prepared texture data"
            }
        }
        if ($configuration.Name -eq 'dx12-fsr2' -and $log -notmatch 'FSR2 2.2.1 Quality:') {
            throw "$name did not activate FSR2 Quality"
        }
        if ($configuration.Name -eq 'dx12-workers' -and $log -notmatch 'draws, peak 4 workers') {
            throw "$name did not exercise four command-recording workers"
        }
        $capture = Get-ChildItem -LiteralPath (Join-Path $directory 'screenshots') -Filter '*.png' |
            Where-Object LastWriteTime -ge $started | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if (-not $capture) { throw "$name missing capture" }
        Copy-Item -LiteralPath $capture.FullName -Destination (Join-Path $evidencePath "$name.png") -Force
        $results += @{ Name = $name; ExecutableSha256 = (Get-FileHash $executable).Hash; Arguments = $arguments; ExitCode = $process.ExitCode }
        Write-Output "$name passed"
    }
}
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidencePath 'runs.json')
