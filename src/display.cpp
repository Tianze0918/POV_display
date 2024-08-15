#include <HardwareSerial.h>
#include <SPIFFS.h>
#include <vector>
#include <string>
#include <SPI.h>
#include <unistd.h>
#include "data_listen.h"

#define WIDTH 314
#define HEIGHT 186
#define ROWINTERVAL 8.84194 
//314 pixels for about 10 cm diameter cylinder with 10pixels per centimerter, at a speed of 7200/minutes. 
//      [(7200/60) (Rotation per second) * (10cm*pi) (diameter)] / [(0.1 cm) (width of one pixel)] = 37699.1118431 (At such speed, in every one second, 37699.118431 number of pixel is passed through)
//      1/37699.118431 = 0.00002652582 seconds pass for every one pixel. Since there are 3 arms rotating, the frequency thus should be 0.00002652582/3=0.00000884194 seconds per pixel = 8.84194 microseconds
#define MOSI_PIN 23  // Master Out Slave In (SDI)
#define SCK_PIN 18   // Serial Clock (CLK)
#define LE1_PIN 10   // Latch Enable 1 (LE1)
#define LE2_PIN 11   // Latch Enable 2 (LE2)
#define PWCK_PIN 12  // Pulse Width Clock (PWCK), optional based on your usage


//Mode initialization
enum Mode {
    MENU,
    CHARACTERS,
    PICTURES,
    VIDEOS
};
enum SubMode {
    NONE,
    ROTATING_TEXT,
    STATIC_TEXT
};


//Mode Constant
Mode currentMode = MENU;
SubMode currentSubMode = NONE;
bool videoPlaying = false;


// File and index structure
std::vector<std::string> fileList;
int currentIndex = -1;


// RGB structure and arrays/columns of RGB representing a picture
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB;
RGB def[HEIGHT][WIDTH] = {0};       //Default to be displayed, no led light at all.
RGB current[HEIGHT][WIDTH];
RGB* currentImage=NULL;


//Controller representation used for uploading ddata in
typedef struct {
    uint16_t pinData[16]; // Array to hold the concatenated data for the 16 pins
} LEDController;
LEDController set1[12];
LEDController set2[11];
LEDController set3[11];
LEDController set4;


//video playing mode constants
size_t currentFrame=0;
size_t maxFrame = inMemoryStorage.size();
bool play=false;






// Function to initialize an LEDController
//index here represent which one out of the 12 controller are we initializing
//reverse orientation, 15 represent the top, 0 represent the bottom led.
//There are intotal 11 set of controller with full 16 led, and one set with 10 led
//The top eight bit of pindata is set to the actual color, the lower 8 bit is just zeros to fit
void initializeController1(RGB ledColumn[186]) {
    for (int i = 0; i < 12; i++) {
        set1[i].pinData[0]  = (ledColumn[1  + 16 * i - 1].g << 8);
        set1[i].pinData[1]  = (ledColumn[1  + 16 * i - 1].r << 8);
        set1[i].pinData[2]  = (ledColumn[2  + 16 * i - 1].r << 8);
        set1[i].pinData[3]  = (ledColumn[2  + 16 * i - 1].g << 8);
        set1[i].pinData[4]  = (ledColumn[1  + 16 * i - 1].b << 8);
        set1[i].pinData[5]  = (ledColumn[2  + 16 * i - 1].b << 8);
        set1[i].pinData[6]  = (ledColumn[3  + 16 * i - 1].g << 8);
        set1[i].pinData[7]  = (ledColumn[3  + 16 * i - 1].b << 8);
        set1[i].pinData[8]  = (ledColumn[4  + 16 * i - 1].b << 8);
        set1[i].pinData[9]  = (ledColumn[5  + 16 * i - 1].b << 8);
        set1[i].pinData[10] = (ledColumn[5 + 16 * i - 1].r << 8);
        set1[i].pinData[11] = (ledColumn[6 + 16 * i - 1].g << 8);
        set1[i].pinData[12] = (ledColumn[5 + 16 * i - 1].g << 8);
        set1[i].pinData[13] = (ledColumn[4 + 16 * i - 1].r << 8);
        set1[i].pinData[14] = (ledColumn[4 + 16 * i - 1].g << 8);
        set1[i].pinData[15] = (ledColumn[3  + 16 * i - 1].r << 8);
    }
}

void initializeController2(RGB ledColumn[186]) {
    for (int i = 0; i < 11; i++) {
        set2[i].pinData[0]  = (ledColumn[8  + 16 * i - 1].r << 8);
        set2[i].pinData[1]  = (ledColumn[8  + 16 * i - 1].g << 8);
        set2[i].pinData[2]  = (ledColumn[7  + 16 * i - 1].g << 8);
        set2[i].pinData[3]  = (ledColumn[6  + 16 * i - 1].r << 8);
        set2[i].pinData[4]  = (ledColumn[6  + 16 * i - 1].b << 8);
        set2[i].pinData[5]  = (ledColumn[7  + 16 * i - 1].b << 8);
        set2[i].pinData[6]  = (ledColumn[7  + 16 * i - 1].r << 8);
        set2[i].pinData[7]  = (ledColumn[8  + 16 * i - 1].b << 8);
        set2[i].pinData[8]  = (ledColumn[9  + 16 * i - 1].b << 8);
        set2[i].pinData[9]  = (ledColumn[10  + 16 * i - 1].b << 8);
        set2[i].pinData[10] = (ledColumn[11 + 16 * i - 1].b << 8);
        set2[i].pinData[11] = (ledColumn[10 + 16 * i - 1].r << 8);
        set2[i].pinData[12] = (ledColumn[11 + 16 * i - 1].g << 8);
        set2[i].pinData[13] = (ledColumn[10 + 16 * i - 1].g << 8);
        set2[i].pinData[14] = (ledColumn[9 + 16 * i - 1].r << 8);
        set2[i].pinData[15] = (ledColumn[9  + 16 * i - 1].g << 8);
    }
}

void initializeController3(RGB ledColumn[186]) {
    for (int i = 0; i < 11; i++) {
        set3[i].pinData[0]  = (ledColumn[11  + 16 * i - 1].r << 8);
        set3[i].pinData[1]  = (ledColumn[13  + 16 * i - 1].r << 8);
        set3[i].pinData[2]  = (ledColumn[13  + 16 * i - 1].g << 8);
        set3[i].pinData[3]  = (ledColumn[12  + 16 * i - 1].g << 8);
        set3[i].pinData[4]  = (ledColumn[12  + 16 * i - 1].b << 8);
        set3[i].pinData[5]  = (ledColumn[12  + 16 * i - 1].r << 8);
        set3[i].pinData[6]  = (ledColumn[13  + 16 * i - 1].b << 8);
        set3[i].pinData[7]  = (ledColumn[14  + 16 * i - 1].b << 8);
        set3[i].pinData[8]  = (ledColumn[14  + 16 * i - 1].r << 8);
        set3[i].pinData[9]  = (ledColumn[15  + 16 * i - 1].b << 8);
        set3[i].pinData[10] = (ledColumn[16 + 16 * i - 1].b << 8);
        set3[i].pinData[11] = (ledColumn[15 + 16 * i - 1].r << 8);
        set3[i].pinData[12] = (ledColumn[16 + 16 * i - 1].g << 8);
        set3[i].pinData[13] = (ledColumn[16 + 16 * i - 1].r << 8);
        set3[i].pinData[14] = (ledColumn[15 + 16 * i - 1].g << 8);
        set3[i].pinData[15] = (ledColumn[14  + 16 * i - 1].g << 8);
    }
}

void initializeController4(RGB ledColumn[186]) {
    set4.pinData[0]  = (ledColumn[176 + 9  - 1].g << 8);
    set4.pinData[1]  = (ledColumn[176 + 11 - 1].r << 8);
    set4.pinData[2]  = (ledColumn[176 + 10 - 1].g << 8);
    set4.pinData[3]  = (ledColumn[176 + 10 - 1].r << 8);
    set4.pinData[4]  = (ledColumn[176 + 11 - 1].b << 8);
    set4.pinData[5]  = (ledColumn[176 + 10 - 1].b << 8);
    set4.pinData[6]  = (ledColumn[176 + 9  - 1].b << 8);
    set4.pinData[7]  = (ledColumn[176 + 8  - 1].b << 8);
    set4.pinData[8]  = (ledColumn[176 + 8  - 1].r << 8);
    set4.pinData[9]  = (ledColumn[176 + 7  - 1].b << 8);
    set4.pinData[10] = (ledColumn[176 + 6  - 1].b << 8);
    set4.pinData[11] = (ledColumn[176 + 7  - 1].r << 8);
    set4.pinData[12] = (ledColumn[176 + 7  - 1].g << 8);
    set4.pinData[13] = (ledColumn[176 + 8  - 1].g << 8);
    set4.pinData[14] = (ledColumn[176 + 9  - 1].r << 8);
    set4.pinData[15] = (ledColumn[176 + 6  - 1].g << 8);
}


void traverseSPIFFSAndAddFiles(const std::string &directoryPath) {
    // Clear the existing file list
    fileList.clear();

    // Open the root directory
    File cd = SPIFFS.open(directoryPath.c_str());
    if (!cd) {
        Serial.println("Failed to open root directory");
        return;
    }

    // Check if it's a directory
    if (!cd.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    // Open the directory and iterate over all files
    File file = cd.openNextFile();
    while (file) {
        // Add the file name to the fileList vector
        fileList.push_back(file.name());
        Serial.println(file.name());  // Print the file name for debugging

        // Move to the next file
        file = cd.openNextFile();
    }

    // Close the root directory (optional, as it will close automatically when going out of scope)
    cd.close();
}

// Function to load files from a directory
void loadFilesFromDirectory(const std::string &directoryPath) {
    traverseSPIFFSAndAddFiles(directoryPath);

    // Set the initial index to 0 if there are files, otherwise -1
    if (!fileList.empty()) {
        currentIndex = 0;
    } else {
        currentIndex = -1;
    }
}

void displayColumn(RGB ledcolumn[186], int le){     //le here represent which column of the two to latch, for now we are only latching the first one
    //Setup the controller 
    initializeController1(ledcolumn);   //准备好数据
    initializeController2(ledcolumn);
    initializeController3(ledcolumn);
    initializeController4(ledcolumn);

    digitalWrite(LE1_PIN, LOW);  // Ensure LE is low before starting data transfer 把上一个列的颜色熄灭
    digitalWrite(LE2_PIN, LOW); 
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));  // 1 MHz, MSB first, SPI mode 0   传输数据
    for (int i = 0; i < 11; i++) {
        for (int j=0; j<16; j++){
            SPI.transfer16(set1[i].pinData[j]);  // Send 16-bit brightness data for each channel
        }
        for (int j=0; j<16; j++){
            SPI.transfer16(set2[i].pinData[j]);  // Send 16-bit brightness data for each channel
        }
        for (int j=0; j<16; j++){
            SPI.transfer16(set3[i].pinData[j]);  // Send 16-bit brightness data for each channel
        }
    }
    for (int j=0; j<16; j++){
        SPI.transfer16(set1[11].pinData[j]);  // Send 16-bit brightness data for each channel
    }
    for (int j=0; j<10; j++){
        SPI.transfer16(set4.pinData[j]);  // Send 16-bit brightness data for each channel
    }
    
    if (le==1){
        digitalWrite(LE1_PIN, HIGH);  // Set LE high
        digitalWrite(LE1_PIN, LOW); 
    }else{
        digitalWrite(LE2_PIN, HIGH);  // Set LE high
        digitalWrite(LE2_PIN, LOW); 
    }
}

void displayCurrentFile(RGB* file) {
    if (currentIndex >= 0 && currentIndex < fileList.size()) {
        Serial.print("Displaying file: ");
        Serial.println(fileList[currentIndex].c_str());
    } else {
        Serial.println("No file to display.");
        return;
    }

    for (int i=0; i<WIDTH; i++){
        displayColumn(current[i], 1);  
        delayMicroseconds(ROWINTERVAL);        // Small delay in between every column   根据实际情况可以微调
    }
}


int loadRGBFile(const char* directory, const char* filename){
    int size=sizeof(directory)+sizeof(filename);
    char wd[size];
    strcpy(wd, directory); // Copy directory to wd
    strcat(wd, filename);  // Concatenate filename to wd

    FILE *file = fopen(wd, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        return 1;
    }
    size_t bytesRead = fread(current, sizeof(char), sizeof(current) - 1, file);
    fclose(file); // Close the file when done
    return 0;
}

void tryDisplayC(){
    loadFilesFromDirectory("/characters"); // Load character files
    if (currentIndex==-1){
        displayCurrentFile(def[314]);
        Serial.println("No character file is uploaded");
    }else{
        if (loadRGBFile( "/characters", fileList[currentIndex].c_str())!=1){
            displayCurrentFile(current[314]);
        }else{
            perror("Failed to open file");
        }
    }
}

void tryDisplayI(){
    loadFilesFromDirectory("/img"); // Load character files
    if (currentIndex==-1){
        displayCurrentFile(def[314]);
        Serial.println("No img file is uploaded");
    }else{
        if (loadRGBFile( "/img", fileList[currentIndex].c_str())!=1){
            displayCurrentFile(current[314]);
        }else{
            perror("Failed to open file");
        }
    }
}

void playVideo() {
    while (play) {
        uint8_t* temp = inMemoryStorage[currentFrame];
        if (temp == nullptr) {
            Serial.println("Error: some frame within inMemoryStorage points to nullptr");
            break;  //
            play=false;
        }
        RGB* file = reinterpret_cast<RGB*>(temp);
        displayCurrentFile(file);
        currentFrame = (currentFrame + 1) % maxFrame;
    }
}

bool parse_serial_data_and_do_stuff() {
    if (Serial.available() > 0) {
        char input_type = Serial.read();

        switch (currentMode) {
            case MENU:
                if (input_type == 'c') {
                    currentMode = CHARACTERS;
                    Serial.println("Entered Characters mode. Choose 'r' for rotating text or 's' for static text.");
                    tryDisplayC(); //文字显示function
                } else if (input_type == 'p') {
                    currentMode = PICTURES;
                    Serial.println("Entered Pictures mode. Use 'n' for next and 'p' for previous.");
                    tryDisplayI();
                } else if (input_type == 'v') {
                    currentMode = VIDEOS;
                    Serial.println("Entered Videos mode. Use 's' to start and 'p' to pause playback.");
                } else {
                    Serial.println("Unrecognized command in General Menu.");
                }
                break;

            case CHARACTERS:
                if (input_type == 'n') {
                    if (currentIndex < fileList.size() - 1) {
                        currentIndex++;
                        tryDisplayC();
                    } else {
                        Serial.println("No more files.");
                    }
                } else if (input_type == 'p') {
                    if (currentIndex > 0) {
                        currentIndex--;
                        tryDisplayC();
                    } else {
                        Serial.println("No previous files.");
                    }
                } else if (input_type == 'm' || input_type == 'q') {
                    currentMode = MENU;
                    currentSubMode = NONE;
                    Serial.println("Returning to General Menu.");
                } else {
                    Serial.println("Unrecognized command in Characters mode.");
                }
                tryDisplayC();
                break;

            case PICTURES:
                if (input_type == 'n') {
                    if (currentIndex < fileList.size() - 1) {
                        currentIndex++;
                        tryDisplayI();
                    } else {
                        Serial.println("No more pictures.");
                    }
                } else if (input_type == 'p') {
                    if (currentIndex > 0) {
                        currentIndex--;
                        tryDisplayI();
                    } else {
                        Serial.println("No previous pictures.");
                    }
                } else if (input_type == 'm' || input_type == 'q') {
                    currentMode = MENU;
                    currentSubMode = NONE;
                    Serial.println("Returning to General Menu.");
                } else {
                    Serial.println("Unrecognized command in Pictures mode.");
                }
                tryDisplayI();
                break;

            case VIDEOS:
                if (input_type == 's'){ //representing start
                    play=true;
                }else if (input_type == 'p') {//representing pause
                    play=false;
                }else{
                    Serial.println("Unrecognized command in video mode, playing paused");
                    play=false;
                }
                if (play){
                    playVideo();
                }else{
                    Serial.println("Unrecognized command in video mode, playing paused");
                    sleep(3000); 
                }
                break;
        }
    }

    return false;
}


void setupSPI() {
    // Initialize SPI
    SPI.begin(SCK_PIN, -1, MOSI_PIN, -1); // MISO (-1) is not used here, only SCK and MOSI

    // Set the Latch Enable pins as output
    pinMode(LE1_PIN, OUTPUT);
    pinMode(LE2_PIN, OUTPUT);

    // Set the initial state of LE1 and LE2
    digitalWrite(LE1_PIN, LOW); // Start with LE1 low
    digitalWrite(LE2_PIN, LOW); // Start with LE2 low

    // Optionally, set the PWCK pin as output if it's used
    pinMode(PWCK_PIN, OUTPUT);
    digitalWrite(PWCK_PIN, LOW); // Start with PWCK low if needed
}



void setup() {
    Serial.begin(115200);
    Serial.setTimeout(70);
    setupSPI();

    for ( int i = 0; i < 3; ++i ) { Serial.println("Testing Serial.println()"); }
}

void loop() {
    parse_serial_data_and_do_stuff();
}