#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
// #include <omp.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define PAINT_IMPLEMENTATION
#include "paint.h" 

#define MAX_PATH 256
#define OUTPUT_APPEND "seam"

int width, height, stride;

#define AT(arr, row, col) (arr)[(row)*stride + (col)]
#define WITHIN(row, col) (0 <= (col) && (col) < width && 0 <= (row) && (row) < height)
// #define AT(arr, row, col) assert(WITHIN(row, col)); (arr)[(row)*stride + (col)]

float sobel_x[3][3] = {
    {-1.0, 0.0, 1.0},
    {-2.0, 0.0, 2.0},
    {-1.0, 0.0, 1.0}
};

int sobel_y[3][3] = {
    {-1.0, -2.0, -1.0},
    { 0.0,  0.0,  0.0},
    { 1.0,  2.0,  1.0}
};

char* get_default_output_path(char* input_path) {
    char* output_path = calloc(strlen(input_path) + 1 + strlen("seam") + 4 + 1, 1);
    const char* ext = strrchr(input_path, '.');
    strncat(output_path, input_path, ext - input_path);
    strcat(output_path, "-seam.png");

    if (output_path == NULL) {
        fprintf(stderr, "[ERROR] Memory allocation failed\n");
        exit(1);
    }
    return output_path;
}

void argparse(int argc, char** argv, char** input_path, char** output_path, int* iterations, bool* paint) {
    int option;
    while ((option = getopt(argc, argv, ":pi:r:o:")) != -1) {
        switch (option) {
            case 'p':
                *paint = true;
                break;
            case 'i':
                if (strlen(optarg) > MAX_PATH) {
                    fprintf(stderr, "[ERROR] Image path too long\n");
                    exit(1);
                }
                *input_path = optarg;
                break;
            case 'o':
                if (strlen(optarg) > MAX_PATH) {
                    fprintf(stderr, "[ERROR] Output path too long\n");
                    exit(1);
                }
                *output_path = optarg;
                break;
            case 'r':
                *iterations = atoi(optarg);
                break;
            case '?':
                fprintf(stderr, "[ERROR] Unknown option: %c\n", optopt);
                exit(1);
            case ':':
                fprintf(stderr, "[ERROR] Missing argument for option: %c\n", optopt);
                exit(1);
        }
    }

    if (*iterations == 0) {
        fprintf(stderr, "[ERROR] No number of pixel to remove provided\n");
        exit(1);
    }
    if (*iterations < 0) {
        fprintf(stderr, "[ERROR] Cannot remove negative pixels\n");
        exit(1);
    }
    if (*input_path == NULL) {
        fprintf(stderr, "[ERROR] No image path provided\n");
        exit(1);
    }
    if (*output_path == NULL) {
        *output_path = get_default_output_path(*input_path);
    }
}

void convert_to_greyscale(uint32_t* img, float* greyscale) {
    for (int i = 0; i < width * height; ++i) {
        float r = ((img[i] >> (8 * 0)) & 0xFF) / 255.0;
        float g = ((img[i] >> (8 * 1)) & 0xFF) / 255.0;
        float b = ((img[i] >> (8 * 2)) & 0xFF) / 255.0;
        greyscale[i] = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    }
}

float sobel_filter_at(int i, int j, float* greyscale) {
    float gx = 0;
    float gy = 0;

    for (int k = -1; k <= 1; ++k) {
        for (int l = -1; l <= 1; ++l) {
            if (WITHIN(i + k, j + l)) {
                gx += AT(greyscale, (i + k), j + l) * sobel_x[k + 1][l + 1];
                gy += AT(greyscale, (i + k), j + l) * sobel_y[k + 1][l + 1];
            }
        }
    }
    return gx * gx + gy * gy;
}

void apply_sobel_filter(float* gradient, float* greyscale) {
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            AT(gradient, i, j) = sobel_filter_at(i, j, greyscale);
        }
    }
}

void patch_sobel_filter(float* gradient, float* greyscale, int* seam) {
    // #pragma omp simd
    for (int i = 0; i < height; ++i) {
        for (int dj = -2; dj <= 1; ++dj) {
            int j = seam[i] + dj;
            if (WITHIN(i, j)) {
                AT(gradient, i, j) = sobel_filter_at(i, j, greyscale);
            }
        }
    }
}

void apply_dp(float* dp, float* gradient, Pixel_state* states) {
    for (int i = 0; i < width; ++i) {
        dp[i] = gradient[i];
    }

    for (int i = 1; i < height; ++i) {
        // #pragma omp parallel for
        // #pragma omp simd
        for (int j = 0; j < width; ++j) {
            float min_val = AT(dp, (i - 1), j);
            if (WITHIN(i, j - 1) & (min_val > AT(dp, i - 1, j - 1))) {
                min_val = AT(dp, i - 1, j - 1);
            }
            if (WITHIN(i, j + 1) & (min_val > AT(dp, i - 1, j + 1))) {
                min_val = AT(dp, i - 1, j + 1);
            }
            float grad_biased = AT(gradient, i, j);
            if (AT(states, i, j) == PROTECTED) {
                grad_biased += 100000000.0;
            } else if (AT(states, i, j) == WEAK) {
                grad_biased -= 50000000.0f;
            }
            AT(dp, i, j) = grad_biased + min_val;
        }
    }
}

void find_seam(float* dp, int* seam) {
    seam[height - 1] = 0;
    for (int j = 1; j < width; ++j) {
        if (AT(dp, height - 1, j) < AT(dp, height - 1, seam[height - 1])) {
            seam[height - 1] = j;
        }
    }
    for (int i = height - 2; i >= 0; --i) {
        seam[i] = seam[i + 1];
        if (WITHIN(0, seam[i + 1] - 1) && AT(dp, i, seam[i + 1] - 1) < AT(dp, i, seam[i])) {
            seam[i] = seam[i + 1] - 1;
        }
        if (WITHIN(0, seam[i + 1] + 1) && AT(dp, i, seam[i + 1] + 1) < AT(dp, i, seam[i])) {
            seam[i] = seam[i + 1] + 1;
        }
    }

}

void remove_seam(uint32_t* img, float* greyscale, float* gradient, int* seam, Pixel_state* states) {
    for (int i = 0; i < height; ++i) {
        size_t dest_index = i * stride + seam[i];
        size_t len = stride - seam[i] - 1;

        memmove(&img[dest_index], &img[dest_index + 1], len * sizeof(uint32_t));
        memmove(&greyscale[dest_index], &greyscale[dest_index + 1], len * sizeof(float));
        memmove(&gradient[dest_index], &gradient[dest_index + 1], len * sizeof(float));
        memmove(&states[dest_index], &states[dest_index + 1], len * sizeof(Pixel_state));
    }
}


int main(int argc, char** argv) {

    bool paint = false;
    int iterations = 0;
    char* input_path = NULL;
    char* output_path = NULL;
    argparse(argc, argv, &input_path, &output_path, &iterations, &paint);

    uint32_t* pixels = (uint32_t*)stbi_load(input_path, &width, &height, NULL, 4);
    if (pixels == NULL) {
        fprintf(stderr, "[ERROR] Cannot load image %s (%s)\n", input_path, stbi_failure_reason());
        exit(1);
    }
    stride = width;
    if (iterations >= width) {
        fprintf(stderr, "[ERROR] Cannot remove more pixels than the image width\n");
        exit(1);
    }

#ifndef TERSE
    printf("[INFO] Image loaded successfully (width: %d, height: %d)\n", width, height);
    printf("[INFO] Resizing image to: width: %d, height: %d\n", width - iterations, height);
#endif

    uint32_t* img = pixels;
    float* greyscale = malloc(width * height * sizeof(float));
    float* gradient = malloc(width * height * sizeof(float));
    float* dp = malloc(width * height * sizeof(float));
    Pixel_state* states = calloc(width * height, sizeof(Pixel_state));
    int* seam = malloc(height * sizeof(int));
    if (img == NULL || greyscale == NULL || gradient == NULL || dp == NULL || states == NULL || seam == NULL) {
        fprintf(stderr, "[ERROR] Memory allocation failed\n");
        exit(1);
    }
    if (paint) {
        painter(input_path, states);
    }

    convert_to_greyscale(img, greyscale);
    apply_sobel_filter(gradient, greyscale);
    for (int round = 0; round < iterations; ++round) {
        apply_dp(dp, gradient, states);
        find_seam(dp, seam);
        remove_seam(img, greyscale, gradient, seam, states);
        width--;
        patch_sobel_filter(gradient, greyscale, seam);
    }

#ifndef TERSE
    printf("[INFO] Writing output image to %s\n", output_path);
#endif

    if (stbi_write_png(output_path, width, height, 4, pixels, stride * sizeof(uint32_t)) == 0) {
        fprintf(stderr, "[ERROR] Cannot write output image to disk\n");
        exit(1);
    };
#ifndef TERSE
    printf("[INFO] Output image written to disk\n");
#endif
}