$baseDirectory = "."

$sourceFiles = Get-ChildItem -Path $baseDirectory -Recurse -Include *.cpp, *.h -File -ErrorAction SilentlyContinue | Where-Object { 
    $_.FullName -notmatch "build" -and 
    $_.FullName -notmatch "vcpkg_installed" -and
    $_.FullName -notmatch ".git"
}

$fileLineCounts = @()

foreach ($file in $sourceFiles) {
    $lineCount = (Get-Content $file.FullName | Measure-Object -Line -ErrorAction SilentlyContinue).Lines
    if ($lineCount -eq $null) { $lineCount = 0 }
    $fileLineCounts += [PSCustomObject]@{ 
        FilePath = $file.FullName
        LineCount = $lineCount
    }
}

$fileLineCounts | Sort-Object -Property LineCount -Descending | Select-Object -First 20 | Format-Table -AutoSize

