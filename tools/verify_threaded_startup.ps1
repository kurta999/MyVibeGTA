param(
 [switch]$Cancel,
 [string]$CancelAtStage='',
 [string]$Executable=(Join-Path $PSScriptRoot '../build-msvc-ninja/MiniCity3D.exe'),
 [string]$Arguments='--loader-workers=4',
 [string]$OutputPath=''
)
$ErrorActionPreference='Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class StartupLaunchCheck {
 public delegate bool Callback(IntPtr window, IntPtr value);
 [DllImport("user32.dll")] public static extern bool EnumWindows(Callback callback, IntPtr value);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
 [DllImport("user32.dll", CharSet=CharSet.Ansi)] public static extern int GetClassName(IntPtr window, System.Text.StringBuilder text, int count);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr window,uint msg,UIntPtr wp,IntPtr lp,uint flags,uint timeout,out UIntPtr result);
 public static IntPtr Find(uint process,string kind) {
  IntPtr found=IntPtr.Zero;
  EnumWindows((window,value)=>{uint pid;GetWindowThreadProcessId(window,out pid);var name=new System.Text.StringBuilder(256);GetClassName(window,name,256);if(pid==process&&name.ToString()==kind){found=window;return false;}return true;},IntPtr.Zero);
  return found;
 }
}
"@
$executablePath=(Resolve-Path -LiteralPath $Executable).Path
$root=Split-Path -Parent $executablePath
$log=Join-Path $root 'MiniCity3D.log'
$before=(Get-Content -LiteralPath $log).Count
$timer=[Diagnostics.Stopwatch]::StartNew()
$testGame=Start-Process -FilePath $executablePath -WorkingDirectory $root -ArgumentList $Arguments -WindowStyle Hidden -PassThru
$loading=[IntPtr]::Zero
while($timer.Elapsed.TotalSeconds -lt 10 -and $loading -eq [IntPtr]::Zero -and -not $testGame.HasExited){
 $loading=[StartupLaunchCheck]::Find($testGame.Id,'MiniCity3DLoading');if($loading -eq [IntPtr]::Zero){Start-Sleep -Milliseconds 20}
}
if($loading -eq [IntPtr]::Zero){throw 'Actual game did not show the loading window'}
Write-Output ('Actual loading window created after {0:N3} s' -f $timer.Elapsed.TotalSeconds)
$result=[UIntPtr]::Zero
if([StartupLaunchCheck]::SendMessageTimeout($loading,0,[UIntPtr]::Zero,[IntPtr]::Zero,2,1000,[ref]$result) -eq [IntPtr]::Zero){throw 'Loading window did not respond'}
if($Cancel -or $CancelAtStage){
 if($CancelAtStage){
  $stageReached=$false
  while($timer.Elapsed.TotalSeconds -lt 60 -and -not $testGame.HasExited){
   $lines=Get-Content -LiteralPath $log | Select-Object -Skip $before
   if($lines | Where-Object { $_.EndsWith("Startup: $CancelAtStage") }){$stageReached=$true;break}
   Start-Sleep -Milliseconds 20
  }
  if(-not $stageReached){throw "Startup stage was not reached: $CancelAtStage"}
  # Let jobs enter the stage before testing cancellation, rather than closing
  # at the checkpoint that precedes worker submission.
  Start-Sleep -Milliseconds 100
 }
 $cancelTimer=[Diagnostics.Stopwatch]::StartNew()
 [void][StartupLaunchCheck]::SendMessageTimeout($loading,0x10,[UIntPtr]::Zero,[IntPtr]::Zero,2,1000,[ref]$result)
 $testGame.WaitForExit(15000)|Out-Null
 if(-not $testGame.HasExited -or $testGame.ExitCode -ne 0){throw 'Startup cancellation did not exit cleanly'}
 Write-Output ('Actual game cancellation exited cleanly after {0:N3} s; cancellation latency {1:N3} s' -f $timer.Elapsed.TotalSeconds,$cancelTimer.Elapsed.TotalSeconds)
}else{
 $ready=$false
 while($timer.Elapsed.TotalSeconds -lt 60 -and -not $testGame.HasExited){
  $lines=Get-Content -LiteralPath $log | Select-Object -Skip $before
  if($lines -match 'Startup ready in'){$ready=$true;break}
  Start-Sleep -Milliseconds 100
 }
 if(-not $ready){throw 'Actual game did not finish startup'}
 if([StartupLaunchCheck]::Find($testGame.Id,'MiniCity3DLoading') -ne [IntPtr]::Zero){Start-Sleep -Milliseconds 250}
 if([StartupLaunchCheck]::Find($testGame.Id,'MiniCity3DLoading') -ne [IntPtr]::Zero){throw 'Actual loading window did not close at ready'}
 $main=[StartupLaunchCheck]::Find($testGame.Id,'MiniCity3D')
 if($main -eq [IntPtr]::Zero){throw 'Actual game window missing after startup'}
 [void][StartupLaunchCheck]::SendMessageTimeout($main,0x10,[UIntPtr]::Zero,[IntPtr]::Zero,2,2000,[ref]$result)
 $testGame.WaitForExit(10000)|Out-Null
 if(-not $testGame.HasExited -or $testGame.ExitCode -ne 0){throw 'Test game did not close cleanly'}
 Write-Output ('Actual game launch and close passed in {0:N3} s' -f $timer.Elapsed.TotalSeconds)
}
$outputLines=@(Get-Content -LiteralPath $log | Select-Object -Skip $before)
if($OutputPath){$outputLines | Set-Content -LiteralPath $OutputPath}
$outputLines
