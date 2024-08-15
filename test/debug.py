import subprocess

# Path to your ELF file
elf_file = "/Users/zhaoze/Documents/PlatformIO/Projects/POV_display/.pio/build/esp32dev/firmware.elf"

# Path to addr2line tool
addr2line_tool = "/Users/zhaoze/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-addr2line"

# Backtrace addresses
backtrace_addresses = [
    "0x42008bd7",
    "0x42004876",
    "0x42004ce5",
    "0x42002f35",
    "0x42002f93",
    "0x42003a0b",
    "0x4200c892"
]

# Function to run addr2line and get function info
def get_function_info(address):
    command = [addr2line_tool, "-pfiaC", "-e", elf_file, address]
    result = subprocess.run(command, stdout=subprocess.PIPE)
    return result.stdout.decode('utf-8').strip()

# Translate addresses
for address in backtrace_addresses:
    info = get_function_info(address)
    print(f"{address}: {info}")
