#pragma once

#include <stdbool.h>

#define FONT_W 5
#define FONT_H 7

// True when pixel (x, y) of character c is lit. x < FONT_W, y < FONT_H.
bool font5x7_pixel(char c, int x, int y);
