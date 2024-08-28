#ifndef HASH_H_INCLUDED
#define HASH_H_INCLUDED

#include "basic.h"

u64 hash_bytes(void *p, u64 size);
u64 hash_string(char *string);

#endif // HASH_H_INCLUDED
