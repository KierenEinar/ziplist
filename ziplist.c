//// Created by CF on 2021/5/24.//#include <stdio.h>#include <memory.h>#include <limits.h>#include <stdint.h>#include <string.h>#include <stdlib.h>#include "ziplist.h"#define ZIPLIST_END 255#define ZIPLIST_BIG_PREVLEN 254#define ZIP_STR_MASK 0xc0#define ZIP_STR_06B (0<<6)#define ZIP_STR_14B (1<<6)#define ZIP_STR_32B (2<<6)#define ZIP_INT_16B (0xc0 |0<<4)  // 1100 0000#define ZIP_INT_32B (0xc0 |1<<4)  // 1101 0000#define ZIP_INT_64B (0xc0 |2<<4)  // 1110 0000#define ZIP_INT_24B (0xc0 |3<<4)  // 1111 0000#define ZIP_INT_8B  0xfe          // 1111 1110 8bit#define ZIP_INT_IMM_MIN 0xf1      // 1111 0001#define ZIP_INT_IMM_MAX 0xfd      // 1111 1101#define INT24_MAX 0x7fffff       // 24bit 的最大值#define INT24_MIN -INT24_MAX-1  // 24bit 的最小值#define ZIPLIST_HEADER_SIEZ ((sizeof(uint32_t)*2)+(sizeof(uint16_t)))#define ZIPLIST_END_SIZE (sizeof(uint8_t))#define zlbytes(zl) (*((uint32_t*)zl))#define zltail(zl) (*((uint32_t*)(zl+sizeof(uint32_t))))#define zllen(zl) (*((uint16_t*)(zl+sizeof(uint32_t)*2)))#define zlend(zl) (*((uint32_t*)(zl+sizeof(uint32_t)*2+sizeof(uint16_t))))#define intrev32ifbe(v) (v)#define intrev16ifbe(v) (v)#define memrev16ifbe(v) (v)#define memrev32ifbe(v) (v)#define memrev64ifbe(v) (v)#define ZIPLIST_ENTRY_TAIL(zl) (zl+intrev32ifbe(zltail(zl)))#define ZIPLIST_ENTRY_HEAD(zl) (*((unsigned char*)(zl+sizeof(uint32_t)*2+sizeof(uint16_t))))#define ZIPLIST_INCR_LENGTH(zl, incr) do {                      \    if (intrev16ifbe(zllen((zl))) < UINT16_MAX) {               \        zllen((zl)) = intrev16ifbe(zllen((zl)) + (incr));          \    }                                                           \} while(0)#define OK 1#define ERR 0#define ZIP_DECODE_PREVLENSIZE(ptr, prevlensize) do {           \    if ((ptr[0]) < ZIPLIST_BIG_PREVLEN) {                       \        (prevlensize) = 1;                                      \    } else {                                                    \        (prevlensize) = 5;                                      \    }                                                           \} while(0);#define ZIP_IS_STR(encoding) ((encoding) & (ZIP_STR_MASK)) < ZIP_STR_MASK#define ZIP_DECODE_PREVLEN(ptr, prevlensize, prevlen) do {      \    ZIP_DECODE_PREVLENSIZE(ptr, prevlensize)                    \    if ((prevlensize) == 1) {                                     \        (prevlen) = ptr[0];                                       \    } else {                                                    \        memcpy(&(prevlen), (unsigned char*)((ptr)+1), 4);                           \        memrev32ifbe(&prevlen);                                 \    }                                                           \} while(0);unsigned int zipIntSize(unsigned char encoding) {    switch (encoding) {        case ZIP_INT_8B:            return 1;        case ZIP_INT_16B:            return 2;        case ZIP_INT_24B:            return 3;        case ZIP_INT_32B:            return 4;        case ZIP_INT_64B:            return 8;    }    if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX) {        return 0;    }    return -1;}#define ZIP_DECODE_LENGTH(ptr, encoding, lensize, len) do {                     \    (encoding) = ((ptr)[0]);                                                    \    if ((encoding) < ZIP_STR_MASK) {                                            \        if (((encoding) & ZIP_STR_MASK) == ZIP_STR_06B) {                       \            (lensize) = 1;                                                      \            (len) = ((encoding) & 0x3f);                                        \        } else if (((encoding) & ZIP_STR_MASK) == ZIP_STR_14B) {                \            (lensize) = 2;                                                      \            (len) = ((((ptr)[0] & 0x3f) << 8) | (ptr)[1]);                      \        } else if (((encoding) & ZIP_STR_MASK) == ZIP_STR_32B) {                \            (lensize) = 5;                                                      \            (len) = (((ptr)[1] << 24) | ((ptr)[2] << 16) | ((ptr)[3] << 8) | (ptr)[4]);   \        }                                                                       \    } else {                                                                    \        (lensize) = 1;                                                          \        (len) = zipIntSize(encoding);                                           \    }                                                                           \} while(0);// 获取元素ptr的长度unsigned int zipRawEntryLength(unsigned char *ptr) {    unsigned int prevlensize, lensize, len, encoding;    ZIP_DECODE_PREVLENSIZE(ptr, prevlensize)    ZIP_DECODE_LENGTH(ptr+prevlensize, encoding, lensize, len)    return prevlensize + lensize + len;}// 字符串转数字int string2ll(const char *s, size_t slen, long long *value) {    const char *p = s;    size_t plen = 0;    unsigned int negative = 0;    unsigned long long v = 123456789;    // 如果是空字符串    if (plen == slen) {        return ERR;    }    // 如果是'0', 长度是1    if (p[0] == '0' && slen == 1) {        if (value) *value = 0;        return OK;    }    // 处理负数得情况    if (p[0] == '-') {        negative = 1;        p++, plen++;        if (plen == slen) {            return ERR;        }    }    // 处理首位, 必须是 1-9    if (p[0]>='1' && p[0] <= '9') {        v = p[0] - '0';        p++, plen++;    } else {        return ERR;    }    while (plen<slen && p[0]>='0' && p[0] <= '9') {        if (v <= ULLONG_MAX / 10) {            v = v * 10;        }        if (v <= ULLONG_MAX - (p[0]-'0')) {            v = v + (p[0]-'0');        }        p++, plen++;    }    if (plen < slen) {        return ERR;    }    if (!negative) {        if (value) {            if (v>LLONG_MAX) {                return ERR;            }            *value = v;        }        return OK;    }    if (value) {        if (v>((unsigned long long)(LLONG_MAX)+1)) {            return ERR;        }        *value = -(v);    }    return OK;}// 试图转码, 只有数字类型才会, 字符串直接返回int zipTryEncoding (unsigned char *s, unsigned char *encoding, int64_t *v,size_t slen) {    long long value = 0;    if (!string2ll(s, slen, &value)) {        return ERR;    }    if (value>=0 && value<=12) {        *encoding = 0xf0 | (value+1)>>4;    } else if (value>=INT8_MIN && value <= INT8_MAX) {        *encoding = ZIP_INT_8B;    } else if (value>=INT16_MIN && value <= INT16_MAX) {        *encoding = ZIP_INT_16B;    } else if (value>=INT24_MIN && value <= INT24_MAX) {        *encoding = ZIP_INT_24B;    } else if (value>=INT32_MIN && value <= INT32_MAX) {        *encoding = ZIP_INT_32B;    } else if (value>=INT64_MIN && value <= INT64_MAX) {        *encoding = ZIP_INT_64B;    } else {        return ERR;    }    if (v) *v = value;    return OK;}// 存储len超过253int zipStoreEntryPrevLengthLarge(unsigned char *p, size_t prevlen) {    if (p) {        p[0] = ZIPLIST_BIG_PREVLEN;        memcpy(p+1, &prevlen, 4);        memrev32ifbe(p+1);    }    return 5;}// 存储prevlen, 返回prevlensizeint zipStoreEntryPrevLength(unsigned char *p, size_t prevlen) {    if (!p) return prevlen >= ZIPLIST_BIG_PREVLEN ? 5 : 1;    if (prevlen < ZIPLIST_BIG_PREVLEN) {        p[0] = prevlen;        return 1;    }    return zipStoreEntryPrevLengthLarge(p, prevlen);}// 存储encoding, 返回lensizeint zipStoreEntryEncoding(unsigned char *p, unsigned char encoding, size_t rawlen) {    int len = 1, buf[5];    if (ZIP_IS_STR(encoding)) {        if (len <= 0x3f) {            if (!p) return len;            buf[0] = rawlen & 0x3f;        } else if (len <= 0x3fff) {            len+=1;            if (!p) return len;            //    001111 00111000            // 01 000000 00000000            // 01 001111 00111000            buf[0] = (rawlen >> 8 & 0x3f) | ZIP_STR_14B;            buf[1] = rawlen & 0xff;        } else {            len+=4;            if (!p) return len;            buf[0] = ZIP_STR_32B;            buf[1] = rawlen >> 24 & 0xff ;            buf[2] = rawlen >> 16 & 0xff;            buf[3] = rawlen >> 8 & 0xff;            buf[4] = rawlen & 0xff;        }    } else {        if (!p) return len;        buf[0] = encoding;    }    memcpy(p, buf, len);    return 0;}int zipPreLenByteDiff(unsigned char *p, size_t reqlen) {    size_t prevlensize;    ZIP_DECODE_PREVLENSIZE(p, prevlensize)    return zipStoreEntryPrevLength(NULL, reqlen) - prevlensize;}// 小端序存放void zipSaveInteger(unsigned char *p, int64_t value, unsigned char encoding) {    int16_t i16;    int32_t i32;    int64_t i64;    if (encoding == ZIP_INT_8B) {        ((int8_t*)p)[0] = (int8_t)value;    } else if (encoding == ZIP_INT_16B) {        i16 = value;        memcpy(p, &i16, sizeof(i16));        memrev16ifbe(p);    } else if (encoding == ZIP_INT_32B) {        i32 = value;        memcpy(p, &i32, sizeof(i32));        memrev32ifbe(p);    } else if (encoding == ZIP_INT_64B) {        i64 = value;        memcpy(p, &i64, sizeof(i64));        memrev64ifbe(p);    } else if (encoding == ZIP_INT_24B) {        i32 = value << 8;        memrev32ifbe(i32);        memcpy(p, ((uint8_t*)&i32)+1, sizeof(i32) - sizeof(uint8_t));    } else if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX) {        // do nothing    } else {       // 不知道该干嘛    }}int64_t zipLoadInteger(unsigned char *p, unsigned char encoding) {    int16_t i16;    int32_t i32;    int64_t i64, ret;    if (encoding == ZIP_INT_8B) {        ret = ((int8_t*)p)[0];    } else if (encoding == ZIP_INT_16B) {        memcpy(&i16, p, sizeof(i16));        memrev16ifbe(&i16);        ret = i16;    } else if (encoding == ZIP_INT_32B) {        memcpy(&i32, p, sizeof(i32));        memrev32ifbe(&i32);        ret = i32;    } else if (encoding == ZIP_INT_64B) {        memcpy(&i64, p, sizeof(i64));        memrev64ifbe(&i64);        ret = i64;    }    return ret;}typedef struct zlentry {    unsigned int prevrawlensize;    unsigned int prevrawlen;    unsigned int headersize;    unsigned int len;    unsigned int lensize;    unsigned char *p;    unsigned char encoding;}zlentry;// ---------------------API---------------------------void zipEntry(unsigned char *p, zlentry *e) {    ZIP_DECODE_PREVLEN(p, e->prevrawlensize, e->prevrawlen);    ZIP_DECODE_LENGTH(p, e->encoding, e->lensize, e->len)    e->headersize = e->prevrawlensize + e->lensize;    e->p = p;}unsigned char * ziplistResize(unsigned char *zl, size_t resizelen) {    zl = realloc(zl, resizelen);    zlbytes(zl) = intrev32ifbe(resizelen);    zl[resizelen-1] = ZIPLIST_END;    return zl;}// 实例化一个ziplistunsigned char *ziplistNew (void) {    unsigned int bytes = ZIPLIST_HEADER_SIEZ+ZIPLIST_END_SIZE;    unsigned char *zl =malloc(bytes);    zlbytes(zl) = intrev32ifbe(bytes); // 设置bytes    zltail(zl)  = intrev32ifbe(ZIPLIST_HEADER_SIEZ); // 设置tail offset    zllen(zl)   = intrev32ifbe(0); // 设置len    zlend(zl)   = intrev32ifbe(ZIPLIST_END); // 设置end    return zl;}// 连锁更新prevlen, 该过程有可能会扩容unsigned char *__ziplistCascadeUpdate(unsigned char *zl, unsigned char *p) {    zlentry entry, nextEntry;    size_t curlen = intrev32ifbe(zlbytes(zl));    unsigned char *np;    unsigned int rawlen, rawlensize, extra, offset, noffset;    while (p[0]!=ZIPLIST_END) {        zipEntry(p, &entry);        rawlen = entry.headersize + entry.len; // 计算该节点的长度        rawlensize = zipStoreEntryPrevLength(NULL, rawlen); // 计算下个节点存放该节点需要用多少字节        np = p + rawlen; // 获取p的next节点        if (np[0] == ZIPLIST_END) break;        zipEntry(np, &nextEntry);        if (nextEntry.prevrawlen == rawlen) break;        if (nextEntry.prevrawlensize < rawlensize) { // 如果下一个节点prevlensize不够存当前节点的lensize, 那么需要做扩容            extra = rawlensize - nextEntry.prevrawlensize; // 需要扩容的长度            offset = p - zl;            zl = ziplistResize(zl, curlen + extra); // 扩容            p = zl + offset; // 重新获得p            np = p + rawlen; // 重新获取p的next节点            noffset = np - zl;            memmove(np+rawlensize,np+nextEntry.prevrawlensize,                    curlen-noffset-1-nextEntry.prevrawlensize); // 移动nextEntry prevlensize之后的内存块            // 更新tail offset            if (np[nextEntry.headersize+nextEntry.len]!=ZIPLIST_END) {                zltail(zl) = intrev32ifbe(intrev32ifbe(ZIPLIST_ENTRY_TAIL(zl)) + extra);            }            p+=rawlen;            curlen+=extra;        } else { // 下一个节点的行为不做缩容, 直接设置prevlensize            if (nextEntry.prevrawlensize > rawlensize) {                zipStoreEntryPrevLengthLarge(p + rawlen, rawlen);            } else {                zipStoreEntryPrevLength(p+rawlen, rawlen);            }        }    }    return zl;}// 插入unsigned char *__ziplstInsert (unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {    // 先获取prevlensize    /* zlentry定义        a. [prevlensize] => 1bytes或者5vytes, 当是1个bytes的时候, 存储0-253, 当是5个bytes的时候, 第一个bytes恒为254, 剩下4个bytes存prevlen, 大端顺序存储        b. [lensize] => 1bytes的encoding, 1bytes或者4bytes的len              [encoding, len] => 00 xxxxxx 类型是str, 剩下的6个bit存字符串的长度, 大端序                         => 01 pppppp qqqqqqqq, 14个bit为字符串的长度, 大端序                         => 10 pppppp qqqqqqqq ssssssss ffffffff 32个bit为字符串的长度, 大端序                         => 0xc0 | 0 << 4, 2bytes int16 11000000                         => 0xc0 | 1 << 4, 4bytes int32 11010000                         => 0xc0 | 2 << 4, 8bytes int64 11100000                         => 0xc0 | 3 << 4, 24bytes      11110000                         => 0xfe, 1bytes, 低4位存储数字        c. 后边接 content    */    size_t reqlen, curlen;    unsigned int prevlensize, prevlen, offset;    int nextdiff, forcelarge = 0;    unsigned char encoding;    long long value = 123456789;    zlentry tail;    // 如果是插入队尾    if (p[0] == ZIPLIST_END) {        // 判断ziplist是否空元素, 若是的话, prevlensize=1, 值是0        unsigned char *ptail = ZIPLIST_ENTRY_TAIL(zl);        if (ptail[0] != ZIPLIST_END) {            // 获取ptail的lensize和len            prevlen = zipRawEntryLength(p);        }    } else {        ZIP_DECODE_PREVLEN(p, prevlensize, prevlen) // 获取待插入元素p的 prevlen和prevlensize    }    // 获取s的长度    if (zipTryEncoding(s, &encoding, &value, slen)) {        reqlen = zipIntSize(encoding);    } else {        reqlen = slen;    }    // 获取prevlensize    reqlen += zipStoreEntryPrevLength(NULL, prevlen);    // 获取lensize    reqlen += zipStoreEntryEncoding(NULL, encoding, slen);    nextdiff = (p[0] == ZIPLIST_END) ? 0 : zipPreLenByteDiff(p, slen);    if (reqlen+nextdiff<0 && nextdiff == -4) {        forcelarge = 1;        nextdiff = 0;    }    curlen = intrev32ifbe(zlbytes(zl));    offset = p - zl;    // 重新分配内存    zl = ziplistResize(zl, curlen+reqlen+nextdiff);    p = zl + offset; // 重新获取到p    if (p[0] != ZIPLIST_END) { // 插入中间        memmove(p+reqlen, p-nextdiff, curlen-offset+nextdiff-1); // 扩容完成        if (forcelarge) {            // 当forcelarge=1时, 说明被插入元素的prevlensize=5, 但是新插入元素导致被插入元素的prevlensize=1            // 并且 reqlen < 4, 所以这里需要调整prevlensize=5, 才能不会被缩容            zipStoreEntryPrevLengthLarge(p+reqlen, reqlen);        } else {            // 设置p+reqlen的prevlen            zipStoreEntryPrevLength(p+reqlen, reqlen);        }        zltail(zl) = intrev32ifbe(intrev32ifbe(ZIPLIST_ENTRY_TAIL(zl)) + reqlen);        zipEntry(p+reqlen, &tail);        if (p[reqlen+tail.headersize+tail.len] != ZIPLIST_END) {            zltail(zl) = intrev32ifbe(intrev32ifbe(ZIPLIST_ENTRY_TAIL(zl)) + nextdiff);        }    } else {        zltail(zl) = intrev32ifbe(p - zl);    }    // 连锁更新元素的prevlen    if (nextdiff != 0) {        offset = p - zl;        __ziplistCascadeUpdate(zl, p);        p = zl + offset;    }    // 设置插入元素的prevlen和len(encoding)相关    p+=zipStoreEntryPrevLength(p, prevlen);    p+=zipStoreEntryEncoding(p, encoding, slen);    if (ZIP_IS_STR(encoding)) {        memcpy(p, s, slen);    } else {        zipSaveInteger(p, value, encoding);    }    // zl的len自增1    ZIPLIST_INCR_LENGTH(zl, 1);    return NULL;}// 查找数组的第n个元素// 如果n>0正向, 反之反向unsigned char * ziplistIndex (unsigned char *zl, int index) {    unsigned char *p;    size_t prevlen, prevlensize;    // 正向的情况    if (index > 0) {        p = ZIPLIST_ENTRY_HEAD(zl);        while(p[0] != ZIPLIST_END && index--) {            p+=zipRawEntryLength(p);        }        return p[0] == ZIPLIST_END ? NULL : p;    }    // 反向的情况    p = ZIPLIST_ENTRY_TAIL(zl);    index = (-index) - 1;    if (p[0] != ZIPLIST_END) { // 说明队列不为空        ZIP_DECODE_PREVLEN(p, prevlensize, prevlen)        while(prevlen > 0 && index --) {            p-=prevlen;            ZIP_DECODE_PREVLEN(p, prevlensize, prevlen)        }    }    return index > 0 ? NULL : p[0];}// 向后查询unsigned char *ziplistNext (unsigned char *zl, unsigned char *p) {    if (p[0] == ZIPLIST_END) {        return NULL;    }    p+=zipRawEntryLength(p);    if (p[0] != ZIPLIST_END) {        return p;    }    return NULL;}// 向前查询unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p) {    unsigned int prevlen, prevlensize = 0;    if (p[0] == ZIPLIST_END) { // 判断是不是空队列, 队尾情况        unsigned char *tail = ZIPLIST_ENTRY_TAIL(zl);        if (tail == ZIPLIST_END) return NULL; // 空队列情况        return tail; // 反之直接返回tail即可    }    if (p == ZIPLIST_ENTRY_HEAD(zl)) { // 队头情况        return NULL;    }    // 队中情况    ZIP_DECODE_PREVLEN(p, prevlensize, prevlen)    return p-=prevlen;}// push ...unsigned char * ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where) {    unsigned char *p;    p = (where == ZIPLIST_HEAD) ? ZIPLIST_ENTRY_HEAD(zl) : ZIPLIST_ENTRY_TAIL(zl);    return __ziplstInsert(zl, p,s, slen);}// insertunsigned char * ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {    return __ziplstInsert(zl, p, s, slen);}// 获取ziplist的长度unsigned int  ziplistLen (unsigned char *zl) {    unsigned int len = 0;    if (intrev16ifbe(zllen(zl)) < UINT16_MAX) {        len = intrev16ifbe(zllen(zl));    } else {        unsigned char *p = ZIPLIST_ENTRY_HEAD(zl);        while(p[0] != ZIPLIST_END) {            len++;            p+=zipRawEntryLength(p);        }        if (len < UINT16_MAX) { // 重新设置            zllen(zl) = intrev16ifbe(len);        }    }    return len;}// 获取元素存储的数据void ziplistGet(unsigned char *p, unsigned char **sstr, unsigned int *slen, long long *sval) {    zlentry entry;    if (sstr) *sstr = NULL;    zipEntry(p, &entry);    if (ZIP_IS_STR(entry.encoding)) {       if (sstr) {           *sstr = p + entry.headersize;           *slen = entry.len;       }    } else {    }}