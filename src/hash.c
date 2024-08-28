#include "hash.h"

// The below work as long as 1 < BITS < the number of bits in UINT. Otherwise it's undefined behaviour.
#define RotateLeft(UINT, BITS)   (((UINT) << (BITS)) | ((UINT) >> (8*sizeof(UINT) - (BITS))))
#define RotateRight(UINT, BITS)  (((UINT) >> (BITS)) | ((UINT) << (8*sizeof(UINT) - (BITS))))

u64 hash_seed = 0x70710678;

u64 hash_bytes(void *p, u64 size)
// Sean Barrett's version of SipHash. Plus it won't return zero.
{
    u64 seed = hash_seed;

    int siphash_c_rounds = 1;
    int siphash_d_rounds = 1;

    u8 *d = p;

    if (size == 4) {
        u32 hash = d[0] | ((u32)d[1] << 8) | ((u32)d[2] << 16) | ((u32)d[3] << 24);
        hash ^= seed;
        hash = (hash ^ 61) ^ (hash >> 16);
        hash = hash + (hash << 3);
        hash = hash ^ (hash >> 4);
        hash = hash * 0x27d4eb2d;
        hash ^= seed;
        hash = hash ^ (hash >> 15);
        hash = (((u64) hash << 16 << 16) | hash) ^ seed;
        return (hash) ? hash : 1;

    } else if (size == 8 && sizeof(u64) == 8) {
        u64 hash = d[0] | ((u64)d[1] << 8) | ((u64)d[2] << 16) | ((u64)d[3] << 24);
        hash |= ((u64)d[4] | ((u64)d[5] << 8) | ((u64)d[6] << 16) | ((u64)d[7] << 24)) << 16 << 16;
        hash ^= seed;
        hash = (~hash) + (hash << 21);
        hash ^= RotateRight(hash, 24);
        hash *= 265;
        hash ^= RotateRight(hash, 14);
        hash ^= seed;
        hash *= 21;
        hash ^= RotateRight(hash, 28);
        hash += (hash << 31);
        hash = (~hash) + (hash << 18);
        return (hash) ? hash : 1;
    }

    u64 v0 = ((((u64) 0x736f6d65 << 16) << 16) + 0x70736575) ^  seed;
    u64 v1 = ((((u64) 0x646f7261 << 16) << 16) + 0x6e646f6d) ^ ~seed;
    u64 v2 = ((((u64) 0x6c796765 << 16) << 16) + 0x6e657261) ^  seed;
    u64 v3 = ((((u64) 0x74656462 << 16) << 16) + 0x79746573) ^ ~seed;

#define SipRound() \
    do { \
        v0 += v1; v1 = RotateLeft(v1, 13);  v1 ^= v0;  v0 = RotateLeft(v0, 8*sizeof(u8)/2); \
        v2 += v3; v3 = RotateLeft(v3, 16);  v3 ^= v2;                                       \
        v2 += v1; v1 = RotateLeft(v1, 17);  v1 ^= v2;  v2 = RotateLeft(v2, 8*sizeof(u8)/2); \
        v0 += v3; v3 = RotateLeft(v3, 21);  v3 ^= v0;                                       \
    } while (0)

    u64 data, i;
    for (i = 0; i+sizeof(u64) <= size; i+=sizeof(u64), d+=sizeof(u64)) {
        data = d[0] | ((u64)d[1] << 8) | ((u64)d[2] << 16) | ((u64)d[3] << 24);
        data |= ((u64)d[4] | ((u64)d[5] << 8) | ((u64)d[6] << 16) | ((u64)d[7] << 24)) << 16 << 16;
        v3 ^= data;
        for (int j = 0; j < siphash_c_rounds; j++)  SipRound();
        v0 ^= data;
    }
    data = size << (8*sizeof(u8)-8);

    switch (size - i) {
      case 7:  data |= ((u64)d[6] << 24) << 24; // fall through
      case 6:  data |= ((u64)d[5] << 20) << 20; // fall through
      case 5:  data |= ((u64)d[4] << 16) << 16; // fall through
      case 4:  data |= (d[3] << 24); // fall through
      case 3:  data |= (d[2] << 16); // fall through
      case 2:  data |= (d[1] << 8); // fall through
      case 1:  data |= d[0]; // fall through
      case 0:  break;
    }

    v3 ^= data;
    for (int j = 0; j < siphash_c_rounds; j++)  SipRound();
    v0 ^= data;
    v2 ^= 0xff;
    for (int j = 0; j < siphash_d_rounds; j++)  SipRound();
    v0 = v1^v2^v3;
    return (v0) ? v0 : 1;
}

u64 hash_string(char *string)
// Thomas Wang's mix function, via Sean Barrett. Also won't return zero.
{
    u64 seed = hash_seed;
    u64 hash = seed;
    while (*string)  hash = RotateLeft(hash, 9) + (u8)*string++;
    hash ^= seed;
    hash = (~hash) + (hash << 18);
    hash ^= hash ^ RotateRight(hash, 31);
    hash = hash * 21;
    hash ^= hash ^ RotateRight(hash, 11);
    hash += (hash << 6);
    hash ^= RotateRight(hash, 22);
    hash += seed;
    return (hash) ? hash : 1;
}

