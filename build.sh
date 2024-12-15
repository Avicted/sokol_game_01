#!/bin/bash

set -xe

mkdir -p build

INCLUDES="-I./submodules/sokol -I./libs/dbgui -I./shaders"
CFLAGS="-std=c17 -O0"
FLAGS="-ggdb -Wall -Wextra -Werror -Wno-implicit-function-declaration -Wno-error=missing-field-initializers -Wno-int-conversion -Wno-error=missing-braces -Wno-error=return-type -Wno-missing-field-initializers"
LINKER_FLAGS="-lm -ldl -lpthread -lX11 -lGL -lGLU -lXrandr -lXi -lXxf86vm -lXinerama -lXcursor"

cc $INCLUDES -o build/sokol_game_01 src/Main.c $CFLAGS $FLAGS $LINKER_FLAGS

cp -r shaders build/

./sokol-shdc --input shaders/quad-sapp.glsl --output shaders/quad-sapp.glsl.h --slang glsl430:hlsl5:metal_macos

./build/sokol_game_01