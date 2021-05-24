#include <stdio.h>
#include <stdint.h>
#include "ziplist.h"

int main() {
    printf("Hello, World!\n");

    unsigned char *zl = ziplistNew();
    printf("zl bytes = %d\n", *((uint32_t*)zl));
    printf("zl tail offset = %d\n", *((uint32_t*)(zl+sizeof(uint32_t))));
    printf("zl len len = %d\n", *((uint16_t *)(zl+sizeof(uint32_t)*2)));
    printf("zl end = %d\n", *((uint8_t *)(zl+sizeof(uint32_t)*2+sizeof(uint16_t))));
    return 0;
}
