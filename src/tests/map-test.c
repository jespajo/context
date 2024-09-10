#include <time.h>

#include "../map.h"

float randf()
{
    return (float)rand()/(float)RAND_MAX;
}

int main()
{
    // First we fill a buffer with random ascii characters. Then we use this data to fill a dict
    // with key-value pairs. Each key is the string starting at some point in the key_data buffer
    // and continuing to the end of the buffer. The associated value is the first four bytes of
    // the key interpreted as a u32.

    Memory_context *ctx = new_context(NULL);

    s64 max_key_len =   5000;
    s64 num_tests   = 100000;

    int time_seed = time(NULL);
    //printf("time_seed: %d\n", time_seed);
    //fflush(stdout);
    srand(time_seed);

    char *key_data = New(max_key_len+12, char, ctx);
    bool *added    = New(max_key_len   , bool, ctx);
    bool *deleted  = New(max_key_len   , bool, ctx);
    char *tmp      = New(max_key_len+12, char, ctx);

    for (s64 i = 0; i < max_key_len; i++) {
        char rand_char = ' ' + rand() % ('~' - ' ');
        assert(' ' <= rand_char && rand_char <= '~');
        key_data[i] = rand_char;
    }

    Dict(u32) *dict = NewDict(dict, ctx);

    for (s64 t = 0; t < num_tests; t++) {
        s64 index = rand() % max_key_len;
        char *key = &key_data[index];
        u32 value;  memcpy(&value, key, sizeof(value));

        if (!added[index] || deleted[index]) {
            // The key shouldn't be in the dict.
            assert(Get(dict, key) == &dict->vals[-1]);
            assert(*Get(dict, key) == 0);

            // Add the key.
            *Set(dict, key) = value;
            assert(*Get(dict, key) == value);
            added[index] = true;
            deleted[index] = false;
        } else {
            // The key should be in the dict.
            assert(*Get(dict, key) == value);

            // Sometimes just move on.
            if (randf() < 0.5)  continue;

            // Otherwise delete the key.
            s64 old_count = dict->count;
            if (randf() < 0.25) {
                // Copy the key to a temporary buffer and do some operations with the copied key so
                // we know that it's the contents of the string that matters.
                strcpy(tmp, key);
                *Set(dict, tmp) = 5;
                assert(*Get(dict, key) == 5);
                assert(Delete(dict, tmp) == true);
                assert(*Get(dict, tmp) == 0);
                assert(Delete(dict, tmp) == false);
            } else {
                assert(Delete(dict, key) == true);
            }
            assert(dict->count == old_count-1);
            assert(*Get(dict, key) == 0);
            assert(Delete(dict, key) == false);
            deleted[index] = true;
        }
    }

    free_context(ctx);

    return 0;
}
