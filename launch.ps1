# BlueDrop launcher — 直接启动，无需 vcvarsall（运行时不需要编译工具）
$env:PATH = "A:\Qt\6.11.0\msvc2022_64\bin;" + $env:PATH
$exe = "F:\Project\BlueDrop\build\src\BlueDrop.exe"
Start-Process $exe -WorkingDirectory "F:\Project\BlueDrop" -WindowStyle Normal
