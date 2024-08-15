#ifndef DATA_WRITER_H
#define DATA_WRITER_H
#include <Arduino.h>
#include <map>

void setupDataWriter();
void writeFile(String path, String data);
void wait_input();
void readFile(String path);
void processCommand(String data);

// Declare the map as an extern variable
extern std::map<size_t, uint8_t*> inMemoryStorage;

#endif // DATA_WRITER_H
