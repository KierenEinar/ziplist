//// Created by CF on 2021/5/24.//#include <stdio.h>#include <memory.h>#include <limits.h>#include <stdint.h>#include <string.h>#include <stdlib.h>#define ZIPLIST_END 255#define ZIPLIST_BIG_PREVLEN 254#define ZIP_STR_MASK 0xc0#define ZIP_STR_06B (0<<6)#define ZIP_STR_14B (1<<6)#define ZIP_STR_32B (2<<6)#define ZIP_INT_16B (0xc0 |0<<4)  // 1100 0000#define ZIP_INT_32B (0xc0 |1<<4)  // 1101 0000#define ZIP_INT_64B (0xc0 |2<<4)  // 1110 0000#define ZIP_INT_24B (0xc0 |3<<4)  // 1111 0000#define ZIP_INT_8B  0xfe          // 1111 1110 8bit#define ZIP_INT_IMM_MIN 0xf1      // 1111 0001#define ZIP_INT_IMM_MAX 0xfd      // 1111 1101#define ZIPLIST_HEADER_SIEZ ((sizeof(uint32_t)*2)+(sizeof(uint16_t)))#define ZIPLIST_END_SIZE (sizeof(uint8_t))#define zlbytes(zl) (*((uint32_t*)zl))#define zltail(zl) (*((uint32_t*)(zl+sizeof(uint32_t))))#define zllen(zl) (*((uint32_t*)(zl+sizeof(uint32_t)*2)))#define zlend(zl) (*((uint32_t*)(zl+sizeof(uint32_t)*2+sizeof(uint16_t))))#define intrev32ifbe(v) (v)#define memrev32ifbe(v) (v)#define ZIPLIST_ENTRY_TAIL(zl) (zl+intrev32ifbe(zltail(zl)))#define ZIP_DECODE_PREVLENSIZE(ptr, prevlensize) do {           \    if ((ptr[0]) < ZIPLIST_BIG_PREVLEN) {                       \        (prevlensize) = 1;                                      \    } else {                                                    \        (prevlensize) = 5;                                      \    }                                                           \} while(0);#define ZIP_DECODE_PREVLEN(ptr, prevlensize, prevlen) do {      \    ZIP_DECODE_PREVLENSIZE(ptr, prevlensize)                    \    if ((prevlensize) == 1) {                                     \        (prevlen) = ptr[0];                                       \    } else {                                                    \        memcpy(&(prevlen), (unsigned char*)((ptr)+1), 4);                           \        memrev32ifbe(&prevlen);                                 \    }                                                           \} while(0);unsigned int zipIntSize(unsigned char encoding) {    switch (encoding) {        case ZIP_INT_8B:            return 1;        case ZIP_INT_16B:            return 2;        case ZIP_INT_24B:            return 3;        case ZIP_INT_32B:            return 4;        case ZIP_INT_64B:            return 8;    }    if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX) {        return 0;    }    return -1;}#define ZIP_DECODE_LENGTH(ptr, encoding, lensize, len) do {                     \    (encoding) = ((ptr)[0]);                                                    \    if ((encoding) < ZIP_STR_MASK) {                                            \        if (((encoding) & ZIP_STR_MASK) == ZIP_STR_06B) {                       \            (lensize) = 1;                                                      \            (len) = ((encoding) & 0x3f);                                        \        } else if (((encoding) & ZIP_STR_MASK) == ZIP_STR_14B) {                \            (lensize) = 2;                                                      \            (len) = ((((ptr)[0] & 0x3f) << 8) | (ptr)[1]);                      \        } else if (((encoding) & ZIP_STR_MASK) == ZIP_STR_32B) {                \            (lensize) = 5;                                                      \            (len) = (((ptr)[1] << 24) | ((ptr)[2] << 16) | ((ptr)[3] << 8) | (ptr)[4]);   \        }                                                                       \    } else {                                                                    \        (lensize) = 1;                                                          \        (len) = zipIntSize(encoding);                                           \    }                                                                           \} while(0);// 获取元素ptr的长度unsigned int zipRawEntryLength(unsigned char *ptr) {    unsigned int prevlensize, lensize, len, encoding;    ZIP_DECODE_PREVLENSIZE(ptr, prevlensize)    ZIP_DECODE_LENGTH(ptr+prevlensize, encoding, lensize, len)    return prevlensize + lensize + len;}// ---------------------API---------------------------// 实例化一个ziplistunsigned char *ziplistNew (void) {    unsigned int bytes = ZIPLIST_HEADER_SIEZ+ZIPLIST_END_SIZE;    unsigned char *zl =malloc(bytes);    zlbytes(zl) = intrev32ifbe(bytes); // 设置bytes    zltail(zl)  = intrev32ifbe(ZIPLIST_HEADER_SIEZ); // 设置tail offset    zllen(zl)   = intrev32ifbe(0); // 设置len    zlend(zl)   = intrev32ifbe(ZIPLIST_END); // 设置end    return zl;}// 插入unsigned char *__ziplstInsert (unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {    // 先获取prevlensize    /* zlentry定义        a. [prevlensize] => 1bytes或者5vytes, 当是1个bytes的时候, 存储0-253, 当是5个bytes的时候, 第一个bytes恒为254, 剩下4个bytes存prevlen, 大端顺序存储        b. [lensize] => 1bytes的encoding, 1bytes或者4bytes的len              [encoding, len] => 00 xxxxxx 类型是str, 剩下的6个bit存字符串的长度, 大端序                         => 01 pppppp qqqqqqqq, 14个bit为字符串的长度, 大端序                         => 10 pppppp qqqqqqqq ssssssss ffffffff 32个bit为字符串的长度, 大端序                         => 0xc0 | 0 << 4, 2bytes int16 11000000                         => 0xc0 | 1 << 4, 4bytes int32 11010000                         => 0xc0 | 2 << 4, 8bytes int64 11100000                         => 0xc0 | 3 << 4, 24bytes      11110000                         => 0xfe, 1bytes, 低4位存储数字        c. 后边接 content    */    unsigned int prevlensize, prevlen;    // 如果是插入队尾    if (p[0] == ZIPLIST_END) {        // 判断ziplist是否空元素, 若是的话, prevlensize=1, 值是0        unsigned char *ptail = ZIPLIST_ENTRY_TAIL(zl);        if (ptail[0] != ZIPLIST_END) {            // 获取ptail的lensize和len            prevlen = zipRawEntryLength(p);        }    } else {        ZIP_DECODE_PREVLEN(p, prevlensize, prevlen) // 获取待插入元素p的 prevlen和prevlensize    }    return NULL;}