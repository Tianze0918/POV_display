import asyncio
from bleak import BleakScanner, BleakClient

# UUIDs must match those in the ESP32 code
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

async def scan_and_connect():
    devices = await BleakScanner.discover()
    esp32_device = None

    for device in devices:
        if device.name == "ESP32_BLE":
            esp32_device = device
            break

    if not esp32_device:
        print("Devices : ", devices)
        print("ESP32 device not found.")
        return

    print(f"Connecting to {esp32_device.name} ({esp32_device.address})")
    async with BleakClient(esp32_device.address) as client:
        try:
            # Read the initial value of the characteristic
            value = await client.read_gatt_char(CHARACTERISTIC_UUID)
            print(f"Initial value: {value.decode('utf-8')}")

            # Write a new value to the characteristic
            await client.write_gatt_char(CHARACTERISTIC_UUID, b'Hello from Python')
            print("Sent data to ESP32")

            # Read back the value to confirm
            value = await client.read_gatt_char(CHARACTERISTIC_UUID)
            print(f"Updated value: {value.decode('utf-8')}")
        except Exception as e:
            print(f"Failed to communicate with the ESP32: {e}")

if __name__ == "__main__":
    asyncio.run(scan_and_connect())
