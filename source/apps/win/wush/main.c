#include "base/usr/sys.h"
#include "wush.h"

int main(int argc, char **argv) {
    Wush wush;
    unused(argc);
    unused(argv);
    Wush_init(&wush);
    return wush.run(&wush);
}
