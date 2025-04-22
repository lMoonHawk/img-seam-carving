#ifndef PAINT_H
#define PAINT_H

typedef enum {
    NONE,
    PROTECTED,
    WEAK,
} Pixel_state;

void paint_circle(int center_x, int center_y, int radius, Pixel_state state, Pixel_state* states);
void painter(const char* image_path, Pixel_state* states);

#endif // PAINT_H
