#pragma once

int getDebugTextArrayMaxLen();
int getDebugTextDisplayCount();
void addDebugText(const char* format, ...);
void addDebugText(const wchar_t* format, ...);
char* getDebugText(int ordered_index);
void initOnScreenDebugText();

void setDebugTextDisplay(bool setOn);
bool getDebugTextDisplay();