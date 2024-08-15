#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <map>
#include <mutex>


// WiFi credentials
const char* ssid = "ESP32-Access-Point";
const char* password = "12345678";

// Create a server object
WebServer server(80);

// Maximum number of slots and fixed memory block size
const size_t maxPSRAMSlots = 30;
const size_t maxSPIFFSSLots = 30;
const size_t totalSlots = 60;
const size_t fixedBlockSize = 314 * 168 * 3;
size_t currentSlot = 0;

// In-memory storage, used to store images/characters into the flash
std::map<size_t, uint8_t*> inMemoryStorage;

// Volatile storage for slot data addresses, stored in PSRAM used for video playing. 
std::map<size_t, std::string> slotDataAddresses;

// Mutex for protecting shared resources
std::mutex storageMutex;

void eraseAllFilesInSPIFFS() {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        SPIFFS.remove(file.name());
        file = root.openNextFile();
    }
    Serial.println("All files erased in SPIFFS.");
}

void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.println("Client connected to WiFi. Erasing all files in SPIFFS.");
            eraseAllFilesInSPIFFS();
            break;
        default:
            break;
    }
}

void writeFileToRAM(size_t slot, const uint8_t* data, size_t size) {
    if (size > fixedBlockSize) {
        server.send(400, "application/json", "{\"error\":\"Data too large\"}");
        return;
    }

    std::lock_guard<std::mutex> lock(storageMutex);
    bool psramAvailable = psramFound();

    if (slot < maxPSRAMSlots && psramAvailable) {
        if (inMemoryStorage.find(slot) != inMemoryStorage.end()) {
            free(inMemoryStorage[slot]);
        }
        uint8_t* buffer = (uint8_t*)ps_malloc(fixedBlockSize);
        if (buffer != nullptr) {
            memcpy(buffer, data, size);
            inMemoryStorage[slot] = buffer;
            char response[100];
            snprintf(response, sizeof(response), "{\"status\":\"Data written to PSRAM\", \"slot\":%zu}", slot);
            server.send(200, "application/json", response);
            slotDataAddresses[slot] = std::to_string((uintptr_t)buffer);
        } else {
            server.send(500, "application/json", "{\"error\":\"Failed to allocate PSRAM\"}");
        }
    } else if (slot >= maxPSRAMSlots && slot < totalSlots) {
        char path[50];
        snprintf(path, sizeof(path), "/data_%zu.bin", slot);
        File file = SPIFFS.open(path, FILE_WRITE);
        if (!file) {
            server.send(500, "application/json", "{\"error\":\"Failed to open file for writing\"}");
            return;
        }
        if (file.write(data, size) != size) {
            server.send(500, "application/json", "{\"error\":\"Failed to write complete data\"}");
            file.close();
            return;
        }
        file.close();
        char response[100];
        snprintf(response, sizeof(response), "{\"status\":\"Data written to SPIFFS\", \"slot\":%zu}", slot);
        server.send(200, "application/json", response);
        slotDataAddresses[slot] = path;
    } else {
        server.send(400, "application/json", "{\"error\":\"Invalid slot number\"}");
    }
}

void handleNotFound() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
}

void handleUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("UploadStart: %s\n", upload.filename.c_str());
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        Serial.printf("UploadWrite: %s, %u bytes\n", upload.filename.c_str(), upload.currentSize);
        if (upload.currentSize > fixedBlockSize) {
            server.send(400, "application/json", "{\"error\":\"Data too large\"}");
            return;
        }

        Serial.println("Got Over Data Too Large");
        Serial.print("psramFound(): ");
        Serial.println(psramFound());
        Serial.print("maxPSRAMSlots: ");
        Serial.println(maxPSRAMSlots);
        Serial.print("totalSlots: ");
        Serial.println(totalSlots);

        std::lock_guard<std::mutex> lock(storageMutex);
        if (currentSlot < maxPSRAMSlots && psramFound()) {
            Serial.println("Inside currentSlot < maxPSRAMSlots && psramFound()");
            if (inMemoryStorage.find(currentSlot) != inMemoryStorage.end()) {
                free(inMemoryStorage[currentSlot]);
            }
            inMemoryStorage[currentSlot] = (uint8_t*)ps_malloc(fixedBlockSize);
            Serial.println("Passed memory allocation");
            if (inMemoryStorage[currentSlot] != nullptr) {
                memcpy(inMemoryStorage[currentSlot], upload.buf, upload.currentSize);
                slotDataAddresses[currentSlot] = std::to_string((uintptr_t)inMemoryStorage[currentSlot]);
            }
        } else if (currentSlot >= maxPSRAMSlots && currentSlot < totalSlots) {
            Serial.println("Inside (currentSlot >= maxPSRAMSlots && currentSlot < totalSlots)");
            char path[50];
            snprintf(path, sizeof(path), "/data_%zu.bin", currentSlot);
            File file = SPIFFS.open(path, FILE_WRITE);
            if (!file) {
                server.send(500, "application/json", "{\"error\":\"Failed to open file for writing\"}");
                Serial.println("Failed to open file");
                return;
            }
            if (file.write(upload.buf, upload.currentSize) != upload.currentSize) {
                server.send(500, "application/json", "{\"error\":\"Failed to write complete data\"}");
                file.close();
                Serial.println("Written bytes to the file doesn't match uploaded bytes");
                return;
            }
            file.close();
            slotDataAddresses[currentSlot] = path;
        } else {
            server.send(400, "application/json", "{\"error\":\"Invalid slot number\"}");
            Serial.println("Invalid slot number");
            Serial.println(currentSlot);
            return;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("UploadEnd: %s (%u)\n", upload.filename.c_str(), upload.totalSize);
        if (currentSlot < maxPSRAMSlots && psramFound() && inMemoryStorage[currentSlot] != nullptr) {
            char response[100];
            snprintf(response, sizeof(response), "{\"status\":\"success\", \"slot\":%zu}", currentSlot);
            server.send(200, "application/json", response);
        } else if (currentSlot >= maxPSRAMSlots && currentSlot < totalSlots) {
            char path[50];
            snprintf(path, sizeof(path), "/data_%zu.bin", currentSlot);
            File file = SPIFFS.open(path, FILE_READ);
            if (!file || file.size() != upload.totalSize) {
                server.send(500, "application/json", "{\"error\":\"Failed to store complete data\"}");
                return;
            }
            file.close();
            char response[100];
            snprintf(response, sizeof(response), "{\"status\":\"success\", \"slot\":%zu}", currentSlot);
            server.send(200, "application/json", response);
        } else {
            server.send(500, "application/json", "{\"error\":\"Unexpected upload status\"}");
        }

        std::lock_guard<std::mutex> lock(storageMutex);
        currentSlot = (currentSlot + 1) % totalSlots; // Wrap around and overwrite
        if (currentSlot < maxPSRAMSlots) {
            if (inMemoryStorage.find(currentSlot) != inMemoryStorage.end()) {
                free(inMemoryStorage[currentSlot]);
                inMemoryStorage.erase(currentSlot);
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.onEvent(WiFiEvent); // Register the WiFi event handler
    WiFi.softAP(ssid, password);

    if (!psramInit()) {
        Serial.println("PSRAM initialization failed!");
    } else {
        Serial.println("PSRAM initialized.");
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    if (!SPIFFS.begin(true)) {
        Serial.println("Critical error: SPIFFS failed to initialize!");
        while (1); // Halt execution
    }

    // Ensure the /write endpoint handles POST requests
    server.on("/write", HTTP_POST, []() {
        // This lambda is just to acknowledge the POST request
        Serial.println("Received POST request to /write");
        server.send(200, "application/json", "{\"status\":\"Waiting for file upload\"}");
    }, handleUpload); // handleUpload is passed here to handle the file upload

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
}
