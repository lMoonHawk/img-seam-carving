# !/usr/bin/env bash
DEBUG_FLAGS="-Wall -Wextra -fno-omit-frame-pointer -fsanitize=undefined -ggdb -O0"
RELEASE_FLAGS="-O3"
LINK="-lm -lSDL2 -lSDL2main -lSDL2_image -Iinclude"

BUILD_TYPE=${1:-prod}

if [ "$BUILD_TYPE" == "debug" ]; then
    CFLAGS="${DEBUG_FLAGS}"
else
    CFLAGS="${RELEASE_FLAGS}"
fi

set -x
gcc -c src/paint.c -o build/paint.o ${CFLAGS}
gcc src/seam_carving.c build/paint.o -o seam ${CFLAGS} ${LINK} #-DTERSE #-pg #-fopenmp #-march=native #-fopt-info-vec-optimized
