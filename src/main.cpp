#include <iostream>
#include "engine.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main() {
    engine* e = new engine();

    if (e->init()!=0) {return -1;}
    e->loop();



    return 0;
}