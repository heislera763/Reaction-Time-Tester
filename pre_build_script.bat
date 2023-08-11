:: Parse the .rc file for PRODUCTVERSION
for /f "tokens=3" %%a in ('findstr PRODUCTVERSION YourResourceFileName.rc') do set version=%%a
set version=%version:~0,-2%

:: Determine bitness based on configuration
if "$(Platform)"=="x64" (
    set bitness=64Bit
) else if "$(Platform)"=="Win32" (
    set bitness=32Bit
)

:: Rename the .exe file
ren "$(TargetPath)" "$(ProjectName)-v%version%-%bitness%.exe"
