# HEROVR 0.0.78 single-session diagnostics: read-only Windows process + GPU engine sampling.
# No game/VR settings changed. Run in PowerShell while the game is running.
param([int]$Seconds=90,[string]$ProcessName='Spider-Man2',[string]$OutputDir='')
$ErrorActionPreference='Stop'
if($Seconds -lt 15 -or $Seconds -gt 600){throw 'Seconds must be 15..600'}
$root=Split-Path -Parent $PSScriptRoot
if(!$OutputDir){$OutputDir=Join-Path $root ('skyline-capture-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$processes=@(Get-Process | Where-Object { $_.ProcessName -like "*$ProcessName*" })
if(!$processes.Count){throw "Game process matching '$ProcessName' not found. Run Get-Process to identify its name."}
$game=$processes | Sort-Object WorkingSet64 -Descending | Select-Object -First 1
$pidNumber=$game.Id
$meta=[ordered]@{captured_at=(Get-Date).ToString('o');game_pid=$pidNumber;game_process=$game.ProcessName;duration_s=$Seconds;note='GPU Engine counters are utilization estimates, NOT GPU frame time; CPU is process-wide time, NOT main-thread frame time; FPS/reprojection requires runtime telemetry.'}
$live=Join-Path $root 'live.txt'
if(Test-Path $live){Copy-Item -LiteralPath $live -Destination (Join-Path $OutputDir 'live-settings-snapshot.txt')}
$meta | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $OutputDir 'capture-info.json') -Encoding UTF8
$csv=Join-Path $OutputDir 'process-gpu-samples.csv'
$rows=New-Object System.Collections.Generic.List[object]
$prevCpu=$game.TotalProcessorTime.TotalMilliseconds
$prevTime=Get-Date
Write-Host "Sampling PID $pidNumber for $Seconds seconds. Keep the SAME rooftop skyline view."
for($i=0;$i -lt $Seconds;$i++){
  Start-Sleep -Seconds 1
  $now=Get-Date
  $p=Get-Process -Id $pidNumber -ErrorAction SilentlyContinue
  if(!$p){Write-Warning 'Game exited';break}
  $elapsed=($now-$prevTime).TotalMilliseconds
  $cpuMs=$p.TotalProcessorTime.TotalMilliseconds
  $cpuPct=if($elapsed -gt 0){100*($cpuMs-$prevCpu)/$elapsed}else{$null}
  $prevCpu=$cpuMs;$prevTime=$now
  $gpu3d=$null;$gpuAll=$null;$gpuNote=''
  try{
    $samples=(Get-Counter '\GPU Engine(*)\Utilization Percentage' -ErrorAction Stop).CounterSamples
    $matches=@($samples | Where-Object { $_.InstanceName -match ('pid_'+$pidNumber+'_') })
    if($matches.Count){
      $gpuAll=($matches | Measure-Object CookedValue -Sum).Sum
      $gpu3d=($matches | Where-Object { $_.InstanceName -match 'engtype_3D' } | Measure-Object CookedValue -Sum).Sum
    }else{$gpuNote='No GPU engine counters for game PID'}
  }catch{$gpuNote='GPU Engine performance counter unavailable'}
  $rows.Add([pscustomobject]@{
    time=$now.ToString('o');pid=$pidNumber;cpu_process_pct_one_core_100=$cpuPct;
    working_set_mb=[math]::Round($p.WorkingSet64/1MB,1);
    private_memory_mb=[math]::Round($p.PrivateMemorySize64/1MB,1);
    gpu_engine_3d_utilization_sum_pct=$gpu3d;gpu_engine_all_utilization_sum_pct=$gpuAll;
    gpu_counter_note=$gpuNote
  })
  if($i%15 -eq 14){Write-Host "$($i+1) seconds collected"}
}
$rows | Export-Csv -Path $csv -NoTypeInformation -Encoding UTF8
Write-Host "Saved $($rows.Count) samples to $OutputDir"
Write-Host 'IMPORTANT: This DOES NOT measure GPU milliseconds, compositor FPS or reprojection.'
Write-Host 'For the single combined test, record your headset refresh, VD reported FPS and reprojection status with the same timestamps.'
