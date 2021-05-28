#include <stdio.h>
#include <stdint.h>
#include "ziplist.h"
#include <stdlib.h>
#include <memory.h>
int main() {
    printf("Hello, World!\n");

    unsigned char *zl = ziplistNew();
    printf("zl bytes = %d\n", *((uint32_t*)zl));
    printf("zl tail offset = %d\n", *((uint32_t*)(zl+sizeof(uint32_t))));
    printf("zl len len = %d\n", *((uint16_t *)(zl+sizeof(uint32_t)*2)));
    printf("zl end = %d\n", *((uint8_t *)(zl+sizeof(uint32_t)*2+sizeof(uint16_t))));

    char buf[4] = {0x04, 0x03, 0x02, 0x01};
    uint32_t *ptr = &buf;
    printf("%d\n", *ptr);

    unsigned int p;
    memcpy(&p, buf, 4);
    printf("%d\n", p);

    unsigned char *encoding = (unsigned char *)(buf+1);
    printf("%d\n", encoding[0]);

    long long v = 0;
    unsigned  int ok = string2ll("-012\n", 4, &v);
    printf("ok=%d, v=%lld\n", ok, v);

    ok = string2ll("-12", 3, &v);
    printf("ok=%d, v=%lld\n", ok, v);

    v = 0;
    ok = string2ll("-1a34", 5, &v);
    printf("ok=%d, v=%lld\n", ok, v);



    return 0;
}
