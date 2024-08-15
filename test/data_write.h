#ifndef DATA_WRITER_H
#define DATA_WRITER_H
#include <Arduino.h>

void setupDataWriter();
void writeFile(String path, String data);
void wait_input();
void readFile(String path);
void processCommand(String data);

#endif // DATA_WRITER_H
