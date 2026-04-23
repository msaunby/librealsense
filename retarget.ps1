# Retarget all project files to v142
Get-ChildItem -Path 'librealsense.vc12','librealsense.vc14','examples\third_party\glfw' -Recurse -Include '*.vcxproj' | ForEach-Object {
  $content = Get-Content $_.FullName -Raw
  $content = $content -replace 'v120','v142'
  $content = $content -replace 'v140','v142'
  Set-Content $_.FullName -Value $content
  Write-Host "Updated: $($_.Name)"
}
Write-Host "All projects retargeted to v142"
