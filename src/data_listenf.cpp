#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <map>
#include <mutex>
#include "data_listen.h"

// WiFi credentials
const char* ssid = "LingS";
const char* password = "50505050";

// Create a server object
WebServer server(80);

// Maximum number of slots and fixed memory block size
const size_t maxPSRAMSlots = 30;
const size_t maxSPIFFSSLots = 30;
const size_t totalSlots = 60;
const size_t fixedBlockSize = 314 * 168 * 3;
size_t currentSlot = 0;

// In-memory storage, psram
std::map<size_t, uint8_t*> inMemoryStorage;

//Don't know what for
// std::map<size_t, std::string> slotDataAddresses;

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


void createSubdirectories() {
    // Define the paths for the subdirectories
    const char* imageDir = "/img";
    const char* charDir = "/char";

    // Create image subdirectory
    if (!SPIFFS.exists(imageDir)) {
        if (SPIFFS.mkdir(imageDir)) {
            Serial.println("Image directory created successfully.");
        } else {
            Serial.println("Failed to create image directory.");
        }
    } else {
        Serial.println("Image directory already exists.");
    }

    // Create character subdirectory
    if (!SPIFFS.exists(charDir)) {
        if (SPIFFS.mkdir(charDir)) {
            Serial.println("Character directory created successfully.");
        } else {
            Serial.println("Failed to create character directory.");
        }
    } else {
        Serial.println("Character directory already exists.");
    }
}



void handleWrite(const char* directory, const char* data) {
    // Determine the next file index
    int fileIndex = 1;
    File root = SPIFFS.open(directory);
    File file = root.openNextFile();

    while (file) {
        String fileName = String(file.name());
        int lastSlash = fileName.lastIndexOf('/');
        int lastDot = fileName.lastIndexOf('.');
        
        if (lastSlash != -1 && lastDot != -1) {
            int currentIndex = fileName.substring(lastSlash + 1, lastDot).toInt();
            if (currentIndex >= fileIndex) {
                fileIndex = currentIndex + 1;
            }
        }
        
        file = root.openNextFile();
    }

    // Generate the new file name based on the index
    String newFilePath = String(directory) + "/" + String(fileIndex) + ".txt";

    // Open new file for writing
    file = SPIFFS.open(newFilePath.c_str(), FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Write data to the file
    if (file.print(data)) {
        Serial.println("File written successfully");
    } else {
        Serial.println("Write failed");
    }

    // Close the file
    file.close();

    // Optional: Open file for reading and print content
    file = SPIFFS.open(newFilePath.c_str());
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.println("File Content:");
    while (file.available()) {
        Serial.write(file.read());
    }

    // Close the file
    file.close();
}



void handleUpload() {
    HTTPUpload& upload = server.upload();
    bool uploadSuccessful = false;

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

        if (currentSlot < maxPSRAMSlots && psramFound()) {
            Serial.println("Inside currentSlot < maxPSRAMSlots && psramFound()");

            // Free existing memory block if any
            if (inMemoryStorage.find(currentSlot) != inMemoryStorage.end()) {
                free(inMemoryStorage[currentSlot]);
            }

            // Allocate memory and attempt re-allocation if it fails
            uint8_t* newBlock = (uint8_t*)ps_malloc(fixedBlockSize); 
            uint8_t counter = 0;
            size_t slotUsed = currentSlot;

            while (counter < maxPSRAMSlots && newBlock == nullptr) {
                Serial.println("Failed to allocate new memory block in PSRAM. Trying to allocate new space.");
                slotUsed = (slotUsed + 1) % maxPSRAMSlots;

                // Free memory in the next slot
                if (inMemoryStorage.find(slotUsed) != inMemoryStorage.end()) {
                    free(inMemoryStorage[slotUsed]);
                }

                // Attempt to allocate memory again in the freed slot
                newBlock = (uint8_t*)ps_malloc(fixedBlockSize);
                counter++;
            }

            if (newBlock == nullptr) {
                Serial.println("Failed to allocate new memory block in PSRAM after trying all slots.");
                server.send(500, "application/json", "{\"error\":\"Memory allocation failed\"}");
                return;
            }

            // Store the data in the allocated memory block
            inMemoryStorage[slotUsed] = newBlock;
            memcpy(inMemoryStorage[slotUsed], upload.buf, upload.currentSize);

            // Update currentSlot to the next position
            currentSlot = (slotUsed + 1) % maxPSRAMSlots; // Wrap around and overwrite

            uploadSuccessful = true;  // Mark the upload as successful
        } else {
            server.send(500, "application/json", "{\"error\":\"No available PSRAM slots\"}");
            return;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("UploadEnd: %s (%u)\n", upload.filename.c_str(), upload.totalSize);
        if (uploadSuccessful) {
            char response[100];
            snprintf(response, sizeof(response), "{\"status\":\"success\", \"slot\":%zu}", currentSlot);
            server.send(200, "application/json", response);
        } else {
            server.send(500, "application/json", "{\"error\":\"Upload failed\"}");
        }
    }
}

void handleNotFound() {
    server.send(404, "application/json", "{\"error\":\"Not found\"}");
}




void setup() {
    Serial.begin(115200);
    WiFi.onEvent(WiFiEvent); // Register the WiFi event handler
    WiFi.begin(ssid, password);

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

    createSubdirectories();

    // Ensure the /write endpoint handles POST requests
    server.on("/write", HTTP_POST, []() {
        // This lambda is just to acknowledge the POST request
        Serial.println("Received POST request to /write");
        server.send(200, "application/json", "{\"status\":\"Waiting for file upload\"}");
    }, handleUpload); // handleUpload is passed here to handle the file upload

    server.on("/write_char", HTTP_POST, []() {
        Serial.println("Received POST request to /write_char");
        if (server.hasArg("plain")) {
            String data = server.arg("plain");
            handleWrite("/char", data.c_str());
            server.send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        }
    });

    server.on("/write_img", HTTP_POST, []() {
        Serial.println("Received POST request to /write_img");
        if (server.hasArg("plain")) {
            String data = server.arg("plain");
            handleWrite("/img", data.c_str());
            server.send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"No data provided\"}");
        }
    });


    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    //Defined by arduino core which constantly polls for request and send them to destination with predefined routes like /write
    server.handleClient();
}
