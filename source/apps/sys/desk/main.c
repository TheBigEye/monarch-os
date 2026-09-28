/**
 * @file main.c
 * @brief Executable entry point for Desk.
 */
#include "desk.h"

int main(int argc, char **argv) {
    Desk desk;
    unused(argc);
    unused(argv);
    Desk_init(&desk);
    return desk.run(&desk) ? 0 : 1;
}
