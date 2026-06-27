# grep -ri -C 20 "Guru Meditation Error" esp32_cdc.log*
# -r: Searches recursively through the directory.
# -i: Ignores case (matches guru meditation error or Guru Meditation Error).
# -C 20: Context flag. This prints 20 lines before and 20 lines after the crash match.
# This ensures you capture the critical register dump and the Backtrace:
#  lines that follow the error.esp32_cdc.log*: The asterisk matches your active log and all numbered backups (.1, .2, etc.) simultaneously.
# Or:
# grep -ri -C 10 "Backtrace:" esp32_cdc.log*
# grep -ri -C 5 "CORE DUMP START" esp32_cdc.log*

import serial
import time
import sys
import logging
from logging.handlers import RotatingFileHandler

SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200
LOG_FILE = 'esp32_cdc.log'

# Scaled for 200KB/min over multiple days
MAX_BYTES = 20 * 1024 * 1024   # 20 MB chunks
BACKUP_COUNT = 500             # Keeps ~10 GB total (about 35 days of history)

CRASH_KEYWORDS = ("Guru Meditation", "Backtrace:", "CORE DUMP", "abort()")

def setup_logger():
    logger = logging.getLogger("ESP32_Logger")
    logger.setLevel(logging.DEBUG)

    # Standard rotating handler (.1, .2, .3...)
    file_handler = RotatingFileHandler(
        LOG_FILE,
        maxBytes=MAX_BYTES,
        backupCount=BACKUP_COUNT,
        encoding='utf-8'
    )

    formatter = logging.Formatter('[%(asctime)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)

    stream_handler = logging.StreamHandler(sys.stdout)
    stream_handler.setFormatter(formatter)
    logger.addHandler(stream_handler)

    return logger

def start_logging():
    logger = setup_logger()
    logger.info("=== Logging started ===")

    while True:
        try:
            with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1,
                               dsrdtr=False, rtscts=False) as ser:
                logger.info(f"=== Connected to {SERIAL_PORT} ===")
                while True:
                    line = ser.readline()
                    if line:
                        decoded_line = line.decode('utf-8', errors='ignore').rstrip('\r\n')
                        logger.info(decoded_line)
                        if any(kw in decoded_line for kw in CRASH_KEYWORDS):
                            logger.critical(f"!!! CRASH DETECTED: {decoded_line} !!!")
                            print("\a", flush=True)  # terminal bell
        except serial.SerialException as e:
            logger.warning(f"=== SERIAL DISCONNECTED ({e}) — possible reboot ===")
            logger.info("=== Reconnecting... ===")
            time.sleep(0.5)
        except KeyboardInterrupt:
            logger.info("=== Logging stopped by user ===")
            break

if __name__ == '__main__':
    start_logging()
