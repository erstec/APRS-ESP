#!/bin/sh

PYTHON=${PYTHON:-$(which python3 python | head -n 1)}

set -e

show_help() {
cat << EOF
Usage: $(basename $0) [-h] [-p PORT] [-P PYTHON] [-f FIRMWARE] [-l LITTLEFS] [-a FS_ADDR]

First-time flash: erases device and writes firmware + filesystem.
Official release factory .bin files already include the filesystem,
so -l is only needed when firmware and filesystem were built separately.

    -h            Display this help and exit
    -p PORT       Serial port (e.g. /dev/ttyUSB0). If not set, esptool auto-detects.
    -P PYTHON     Alternate python interpreter (default: $PYTHON)
    -f FIRMWARE   Factory firmware .bin (without '-update' suffix)
    -l LITTLEFS   LittleFS .bin file (only needed for separately-built FS)
    -a FS_ADDR    LittleFS flash address (default: 0x810000 for ESP32-S3 / T-TWR)
                  Use 0x290000 for ESP32 4MB boards (esp32dev-sa818-868)

Example (T-TWR Plus, release build — factory bin includes LittleFS):
  $(basename $0) -f firmware-lilygo-t-twr-plus-2.0.bin

Example (separately built FS):
  $(basename $0) -f firmware-lilygo-t-twr-plus-2.0.bin -l littlefs-lilygo-t-twr-plus-2.0.bin

EOF
}

FS_ADDR=0x810000

while getopts ":hp:P:f:l:a:" opt; do
    case "${opt}" in
        h) show_help; exit 0 ;;
        p) export ESPTOOL_PORT=${OPTARG} ;;
        P) PYTHON=${OPTARG} ;;
        f) FILENAME=${OPTARG} ;;
        l) LITTLEFS=${OPTARG} ;;
        a) FS_ADDR=${OPTARG} ;;
        *) echo "Invalid flag."; show_help >&2; exit 1 ;;
    esac
done
shift "$((OPTIND-1))"

[ -z "$FILENAME" -a -n "$1" ] && { FILENAME=$1; shift; }

if [ -z "${FILENAME}" ] || [ ! -f "${FILENAME}" ]; then
    show_help
    echo "Error: firmware file not found: ${FILENAME}"
    exit 1
fi

if echo "${FILENAME}" | grep -q "update"; then
    show_help
    echo "Error: use the factory .bin (without '-update' suffix) for first-time install"
    exit 1
fi

echo "Erasing flash and writing firmware: ${FILENAME}"
"$PYTHON" -m esptool --baud 921600 erase_flash
"$PYTHON" -m esptool --baud 921600 write_flash 0x00 "${FILENAME}"

if [ -n "${LITTLEFS}" ]; then
    if [ ! -f "${LITTLEFS}" ]; then
        echo "Error: LittleFS file not found: ${LITTLEFS}"
        exit 1
    fi
    echo "Writing LittleFS filesystem at ${FS_ADDR}: ${LITTLEFS}"
    "$PYTHON" -m esptool --baud 921600 write_flash "${FS_ADDR}" "${LITTLEFS}"
fi

echo "Done. Restart the device."
exit 0
