@ECHO OFF
setlocal enabledelayedexpansion

set PYTHON=python
set FS_ADDR=0x810000

goto GETOPTS
:HELP
echo Usage: %~nx0 [-h] [-p PORT] [-P PYTHON] [-f FIRMWARE] [-l LITTLEFS] [-a FS_ADDR]
echo.
echo First-time flash: erases device and writes firmware + filesystem.
echo Official release factory .bin files already include the filesystem,
echo so -l is only needed when firmware and filesystem were built separately.
echo.
echo     -h            Display this help and exit
echo     -p PORT       Serial port. If not set, esptool auto-detects.
echo     -P PYTHON     Alternate python interpreter (default: %PYTHON%)
echo     -f FIRMWARE   Factory firmware .bin (without '-update' suffix)
echo     -l LITTLEFS   LittleFS .bin file (only needed for separately-built FS)
echo     -a FS_ADDR    LittleFS flash address (default: 0x810000 for ESP32-S3 / T-TWR)
echo                   Use 0x290000 for ESP32 4MB boards (esp32dev-sa818-868)
echo.
echo Example (T-TWR Plus, release build -- factory bin includes LittleFS):
echo   %~nx0 -f firmware-lilygo-t-twr-plus-2.0.bin
echo.
echo Example (separately built FS):
echo   %~nx0 -f firmware-lilygo-t-twr-plus-2.0.bin -l littlefs-lilygo-t-twr-plus-2.0.bin
goto EOF

:GETOPTS
if /I "%1"=="-h"  goto HELP
if /I "%1"=="--help" goto HELP
if /I "%1"=="-f"  set "FILENAME=%2"  & SHIFT & SHIFT & goto GETOPTS
if /I "%1"=="-l"  set "LITTLEFS=%2"  & SHIFT & SHIFT & goto GETOPTS
if /I "%1"=="-p"  set ESPTOOL_PORT=%2 & SHIFT & SHIFT & goto GETOPTS
if /I "%1"=="-P"  set PYTHON=%2 & SHIFT & SHIFT & goto GETOPTS
if /I "%1"=="-a"  set FS_ADDR=%2 & SHIFT & SHIFT & goto GETOPTS
IF NOT "__%1__"=="____" SHIFT & goto GETOPTS

IF "__%FILENAME%__"=="____" (
    echo Error: firmware file not specified.
    goto HELP
)
IF NOT EXIST "%FILENAME%" (
    echo Error: firmware file not found: %FILENAME%
    goto EOF
)
IF NOT x%FILENAME:update=%==x%FILENAME% (
    echo Error: use the factory .bin ^(without '-update' suffix^) for first-time install.
    goto EOF
)

echo Erasing flash and writing firmware: %FILENAME%
%PYTHON% -m esptool --baud 921600 erase_flash
%PYTHON% -m esptool --baud 921600 write_flash 0x00 "%FILENAME%"

IF NOT "__%LITTLEFS%__"=="____" (
    IF NOT EXIST "%LITTLEFS%" (
        echo Error: LittleFS file not found: %LITTLEFS%
        goto EOF
    )
    echo Writing LittleFS filesystem at %FS_ADDR%: %LITTLEFS%
    %PYTHON% -m esptool --baud 921600 write_flash %FS_ADDR% "%LITTLEFS%"
)

echo Done. Restart the device.
:EOF
