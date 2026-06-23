import subprocess
import configparser
import traceback
import sys
import os
from os.path import join, isabs
from readprops import readProps

Import("env")
platform = env.PioPlatform()

def _get_fs_offset(env):
    csv_path = env.subst("$PARTITIONS_TABLE_CSV")
    if not isabs(csv_path):
        csv_path = join(env.subst("$PROJECT_DIR"), csv_path)
    try:
        with open(csv_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = [p.strip() for p in line.split(',')]
                if len(parts) >= 4 and parts[2] in ('spiffs', 'littlefs', 'fat'):
                    return int(parts[3], 16)
    except Exception as e:
        print("Warning: could not read partition CSV {}: {}".format(csv_path, e))
    return None

def esp32_create_combined_bin(source, target, env):
    # this sub is borrowed from ESPEasy build toolchain. It's licensed under GPL V3
    # https://github.com/letscontrolit/ESPEasy/blob/mega/tools/pio/post_esp32.py
    print("Generating combined binary for serial flashing")

    app_offset = 0x10000

    new_file_name = env.subst("$BUILD_DIR/${PROGNAME}.factory.bin")
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    chip = env.get("BOARD_MCU")
    flash_size = env.BoardConfig().get("upload.flash_size")
    flash_freq = env.BoardConfig().get("build.f_flash", '40m')
    flash_freq = flash_freq.replace('000000L', 'm')
    flash_mode = env.BoardConfig().get("build.flash_mode", "dio")
    memory_type = env.BoardConfig().get("build.arduino.memory_type", "qio_qspi")
    if flash_mode == "qio" or flash_mode == "qout":
        flash_mode = "dio"
    if memory_type == "opi_opi" or memory_type == "opi_qspi":
        flash_mode = "dout"
    cmd = [
        "--chip", chip,
        "merge_bin",
        "-o", new_file_name,
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size,
    ]

    print("    Offset | File")
    for item in env.get("FLASH_EXTRA_IMAGES", []):
        if isinstance(item, (list, tuple)):
            # espressif32 6.x: list of (addr, file) tuples
            sect_adr, sect_file = str(item[0]), env.subst(str(item[1]))
        else:
            # espressif32 5.x: "addr file" strings
            sect_adr, sect_file = env.subst(str(item)).split(" ", 1)
        print(" -  {} | {}".format(sect_adr, sect_file))
        cmd += [sect_adr, sect_file]

    print(" - {} | {}".format(hex(app_offset), firmware_name))
    cmd += [hex(app_offset), firmware_name]

    littlefs_bin = join(env.subst("$BUILD_DIR"), "littlefs.bin")
    fs_offset = _get_fs_offset(env)
    if os.path.exists(littlefs_bin) and fs_offset is not None:
        print(" -  {} | {}".format(hex(fs_offset), littlefs_bin))
        cmd += [hex(fs_offset), littlefs_bin]
    else:
        print("   (LittleFS not included — run buildfs target to add)")

    print('Using esptool.py arguments: %s' % ' '.join(cmd))

    esptool.main(cmd)


def esp32_merge_littlefs(source, target, env):
    """Re-create factory bin with LittleFS after buildfs completes."""
    firmware_bin = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    if not os.path.exists(firmware_bin):
        print("Skipping full factory merge: build firmware first ('pio run')")
        return
    esp32_create_combined_bin(source, target, env)


if (platform.name == "espressif32"):
    sys.path.append(join(platform.get_package_dir("tool-esptoolpy")))
    import esptool
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_combined_bin)
    env.AddPostAction("$BUILD_DIR/littlefs.bin", esp32_merge_littlefs)

Import("projenv")

prefsLoc = projenv["PROJECT_DIR"] + "/version.properties"
verObj = readProps(prefsLoc)
print("Using APRS-ESP platformio-custom.py, firmware version " + verObj['long'])

# General options that are passed to the C and C++ compilers
projenv.Append(CCFLAGS=[
    "-DAPP_VERSION=" + verObj['long'],
    "-DAPP_VERSION_SHORT=" + verObj['short']
])
