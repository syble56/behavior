$root = "D:\syble\behavior_SDK\src\behavior_sdk"

$renameMap = @{}
foreach ($dir in @("src", "demo")) {
    Get-ChildItem -Recurse -Include *.h,*.cpp -Path "$root\$dir" | ForEach-Object {
        $oldName = $_.Name
        $base = [System.IO.Path]::GetFileNameWithoutExtension($oldName)
        $ext = $_.Extension
        $snake = $base -creplace '(?<!^)([A-Z])', '_$1'
        $snake = $snake.ToLower()
        $newName = "$snake$ext"
        if ($oldName -ne $newName) {
            $renameMap[$oldName] = $newName
        }
    }
}

Write-Host "=== File rename mapping ==="
foreach ($k in ($renameMap.Keys | Sort-Object)) {
    Write-Host "  $k -> $($renameMap[$k])"
}

$allFiles = @()
$allFiles += Get-ChildItem -Recurse -Include *.h,*.cpp -Path "$root\src","$root\demo"
$allFiles += Get-ChildItem -Path "$root\CMakeLists.txt"
$allFiles += Get-ChildItem -Path "$root\demo\CMakeLists.txt"

foreach ($f in $allFiles) {
    $content = Get-Content $f.FullName -Raw
    $modified = $false
    foreach ($old in $renameMap.Keys) {
        $new = $renameMap[$old]
        if ($content.Contains($old)) {
            $content = $content.Replace($old, $new)
            $modified = $true
        }
    }
    if ($modified) {
        Set-Content $f.FullName $content -NoNewline
        Write-Host "Updated: $($f.FullName.Replace($root + '\',''))"
    }
}

foreach ($dir in @("src", "demo")) {
    Get-ChildItem -Recurse -Include *.h,*.cpp -Path "$root\$dir" | ForEach-Object {
        $oldName = $_.Name
        if ($renameMap.ContainsKey($oldName)) {
            $newName = $renameMap[$oldName]
            $newPath = Join-Path $_.DirectoryName $newName
            Rename-Item $_.FullName $newPath
        }
    }
}

Write-Host ""
Write-Host "$($renameMap.Count) files renamed."
