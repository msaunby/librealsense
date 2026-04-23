# Retarget projects to Windows SDK 10.0
Get-ChildItem -Path 'librealsense.vc14' -Recurse -Include '*.vcxproj' | ForEach-Object {
  $content = Get-Content $_.FullName -Raw
  if ($content -match '8\.1') {
    $content = $content -replace '<WindowsTargetPlatformVersion>8\.1</WindowsTargetPlatformVersion>', '<WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>'
    Set-Content $_.FullName -Value $content
    Write-Host "Updated SDK target: $($_.Name)"
  }
}
Write-Host "SDK retargeting complete"
