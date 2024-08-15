#include <Arduino.h>
#include <LittleFS.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "data_write.h"

#define SERVICE_UUID             "4fafc201-1fb5-459e-8fcc-c5c9c331914b"  // Random UUID for service
#define COMMAND_RESULT_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Random UUID for command/result characteristic
#define NOTIFY_CHARACTERISTIC_UUID "c5c9c3d2-1fb5-459e-8fcc-c5c9c331914b"  // Random UUID for notification characteristic

BLEServer* pServer = NULL;
BLECharacteristic* pCommandResultCharacteristic = NULL;
BLECharacteristic* pNotifyCharacteristic = NULL;
String inputBuffer = "";

void setupDataWriter() {
    if (!LittleFS.begin()) {
        pNotifyCharacteristic->setValue("LittleFS Mount Failed");
        pNotifyCharacteristic->notify();
        return;
    }
    pNotifyCharacteristic->setValue("LittleFS initialized.");
    pNotifyCharacteristic->notify();
}

void writeFile(String path, String data) {
    File file = LittleFS.open(path, FILE_WRITE);
    if (!file) {
        pNotifyCharacteristic->setValue("Failed to open file for writing");
        pNotifyCharacteristic->notify();
        return;
    }
    if (file.print(data)) {
        pNotifyCharacteristic->setValue("Data written to LittleFS");
        pNotifyCharacteristic->notify();
    } else {
        pNotifyCharacteristic->setValue("Write failed");
        pNotifyCharacteristic->notify();
    }
    file.close();
}

void readFile(String path) {
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
        pNotifyCharacteristic->setValue("Failed to open file for reading");
        pNotifyCharacteristic->notify();
        return;
    }

    String result = "";
    while (file.available()) {
        result += (char)file.read();
    }

    file.close();
    pCommandResultCharacteristic->setValue(result.c_str());
    pCommandResultCharacteristic->notify();
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
    pNotifyCharacteristic->setValue(String("Listing directory: " + String(dirname)).c_str());
    pNotifyCharacteristic->notify();

    File root = fs.open(dirname);
    if (!root) {
        pNotifyCharacteristic->setValue("Failed to open directory");
        pNotifyCharacteristic->notify();
        return;
    }
    if (!root.isDirectory()) {
        pNotifyCharacteristic->setValue("Not a directory");
        pNotifyCharacteristic->notify();
        return;
    }

    File file = root.openNextFile();
    String result = "";
    while (file) {
        if (file.isDirectory()) {
            result += "DIR : " + String(file.name()) + "\n";
            if (levels) {
                listDir(fs, file.name(), levels - 1);
            }
        } else {
            result += "FILE: " + String(file.name()) + " SIZE: " + String(file.size()) + "\n";
        }
        file = root.openNextFile();
    }

    pCommandResultCharacteristic->setValue(result.c_str());
    pCommandResultCharacteristic->notify();
}

void processCommand(String data) {
    int firstColon = data.indexOf(':');
    int secondColon = data.indexOf(':', firstColon + 1);

    if (firstColon == -1 || secondColon == -1) {
        pNotifyCharacteristic->setValue("Invalid command format");
        pNotifyCharacteristic->notify();
        return;
    }

    String command = data.substring(0, firstColon);
    String fileName = data.substring(firstColon + 1, secondColon);
    String content = data.substring(secondColon + 1);

    if (command.equals("WRITE")) {
        pNotifyCharacteristic->setValue("Write received");
        pNotifyCharacteristic->notify();
        writeFile(fileName, content);
    } else if (command.equals("READ")) {
        pNotifyCharacteristic->setValue("READ received");
        pNotifyCharacteristic->notify();
        readFile(fileName);
    } else if (command.equals("LIST")) {
        pNotifyCharacteristic->setValue("LIST received");
        pNotifyCharacteristic->notify();
        listDir(LittleFS, "/", 0);
    } else {
        pNotifyCharacteristic->setValue("Unknown command");
        pNotifyCharacteristic->notify();
    }
}

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            inputBuffer += rxValue.c_str();

            int newlineIndex = inputBuffer.indexOf('\n');
            while (newlineIndex != -1) {
                String command = inputBuffer.substring(0, newlineIndex);
                processCommand(command);
                inputBuffer = inputBuffer.substring(newlineIndex + 1);
                newlineIndex = inputBuffer.indexOf('\n');
            }
        }
    }
};

void bleTask(void * parameter) {
    BLEDevice::init("ESP32_BLE");
    pServer = BLEDevice::createServer();
    BLEService* pService = pServer->createService(SERVICE_UUID);

    pCommandResultCharacteristic = pService->createCharacteristic(
        COMMAND_RESULT_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pNotifyCharacteristic = pService->createCharacteristic(
        NOTIFY_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pCommandResultCharacteristic->setCallbacks(new MyCallbacks());
    pCommandResultCharacteristic->addDescriptor(new BLE2902());
    pNotifyCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Waiting for a client connection to notify...");

    // Keep the task running
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void wait_input() {
    // Setup the data writer
    setupDataWriter();


    // Main task loop
    while (true) {
        // Perform other tasks here if needed
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    wait_input();
}

void loop() {
}