// Author: Jonathan Decker
// Description: simple PNG reader/writer using miniz.c
// Simple min sum filtering scheming as defined by PNG spec

#pragma once

typedef union ccpngcol
{
    struct
    {
        unsigned int r : 8;  // Red:     0/255 to 255/255
        unsigned int g : 8;  // Green:   0/255 to 255/255
        unsigned int b : 8;  // Blue:    0/255 to 255/255
        unsigned int a : 8;  // Alpha:   0/255 to 255/255
    };
    unsigned int c;
}ccpng_color;

void CCPNGInit();
unsigned int* CCPNGReadFile(const char* filename, unsigned int* width, unsigned int* height);
void CCPNGWriteFile(const char* filename, unsigned int* data, const unsigned int width, const unsigned int height, const int alpha, const int filter);
void CCPNGDestroy();