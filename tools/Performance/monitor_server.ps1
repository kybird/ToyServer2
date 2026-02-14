param (
    [string]$ProcessName = "VampireSurvivorServer",
    [string]$OutFile = "server_metrics.csv"
)

# Headers
"Timestamp,CPU(%),RAM(MB),ThreadCount,HandleCount" | Out-File -FilePath $OutFile -Encoding UTF8

$cpuCounter = "\Process($ProcessName)\% Processor Time"
$memCounter = "\Process($ProcessName)\Working Set - Private"
$threadCounter = "\Process($ProcessName)\Thread Count"
$handleCounter = "\Process($ProcessName)\Handle Count"

Write-Host "Monitoring $ProcessName... Output: $OutFile"

$hasStarted = $false
$maxFetches = 0

while ($true) {
    try {
        if ((Get-Process -Name $ProcessName -ErrorAction SilentlyContinue) -eq $null) {
            if ($hasStarted) {
                Write-Host "Process $ProcessName has stopped. Monitor exiting."
                break
            }
            if ($maxFetches -gt 10) {
                 Write-Host "Process $ProcessName never started. Monitor exiting."
                 break
            }
            Write-Host "Process $ProcessName not found. Waiting..."
            
            $maxFetches++
            Start-Sleep -Seconds 1
            continue
        }

        $hasStarted = $true
        $maxFetches = 0

        $cpu = (Get-Counter $cpuCounter -ErrorAction SilentlyContinue).CounterSamples.CookedValue
        $mem = (Get-Counter $memCounter -ErrorAction SilentlyContinue).CounterSamples.CookedValue / 1MB
        $threads = (Get-Counter $threadCounter -ErrorAction SilentlyContinue).CounterSamples.CookedValue
        $handles = (Get-Counter $handleCounter -ErrorAction SilentlyContinue).CounterSamples.CookedValue
        
        $timestamp = Get-Date -Format "HH:mm:ss"
        
        "$timestamp,$($cpu.ToString('F2')),$($mem.ToString('F2')),$threads,$handles" | Out-File -FilePath $OutFile -Append -Encoding UTF8
        Write-Host "[$timestamp] CPU: $($cpu.ToString('F0'))% | RAM: $($mem.ToString('F0')) MB | Thr: $threads"
    }
    catch {
        Write-Host "Error reading counters: $_"
    }
    
    Start-Sleep -Seconds 1
}
