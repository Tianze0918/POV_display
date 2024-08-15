#include <Arduino.h>
#include <LittleFS.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "data_write.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"  // Random UUID for service
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Random UUID for characteristic

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
String inputBuffer = "";

void setupDataWriter() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    Serial.println("LittleFS initialized.");
}

void writeFile(String path, String data) {
    File file = LittleFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(data)) {
        Serial.println("Data written to LittleFS");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void processCommand(String data) {
    int firstColon = data.indexOf(':');
    int secondColon = data.indexOf(':', firstColon + 1);

    if (firstColon == -1 || secondColon == -1) {
        Serial.println("Invalid command format");
        return;
    }

    String command = data.substring(0, firstColon);
    String fileName = data.substring(firstColon + 1, secondColon);
    String content = data.substring(secondColon + 1);

    if (command.equals("WRITE")) {
        writeFile(fileName, content);
    } else if (command.equals("READ")) {
        readFile(fileName);
    } else {
        Serial.println("Unknown command");
    }
}

void readFile(String path) {
    File file = LittleFS.open(path, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    while (file.available()) {
        Serial.write(file.read());
    }

    file.close();
    Serial.println("Read complete");
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

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE
    );

    pCharacteristic->setCallbacks(new MyCallbacks());
    pCharacteristic->setValue("Hello World");
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

void wait_input(void *parameter) {
    // Setup the data writer
    setupDataWriter();

    // Create the BLE task
    xTaskCreate(bleTask, "BLE Task", 8192, NULL, 1, NULL);

    // Main task loop
    while (true) {
        // Perform other tasks here if needed
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

