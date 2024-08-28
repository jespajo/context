//|Todo: Move documentation from new_map() to header file.

#include "map.h"
#include "hash.h"

typedef Map(char, char) Alias_map;

void *new_map(Memory_context *context, u64 key_size, u64 val_size, bool string_mode)
{
    s64 INITIAL_KV_LIMIT    = 8; // Limit on key/value pairs. Includes the first one which is reserved, accessed as keys[-1] and vals[-1].
    s64 INITIAL_NUM_BUCKETS = 8;

    Alias_map *map = New(Alias_map, context);

    map->context      = context;

    map->key_size     = key_size;
    map->val_size     = val_size;

    map->string_mode  = string_mode;

    map->limit        = INITIAL_KV_LIMIT;
    map->keys         = alloc(map->limit, key_size, context);
    map->vals         = alloc(map->limit, val_size, context);

    // Reserve the first key/value pair. keys[-1] will be used as temporary storage for a key we're
    // operating on with Get(), Set() or Delete(). vals[-1] will store the default value for *Get()
    // to return if the requested key is not present.
    //
    // This means that when you use Get() with an incorrect key, the result returned is a zeroed-out
    // value of the same type as map's values. We rely on this behaviour elsewhere! One day we might
    // want a SetDefault() to allow changing the default value returned on a per-map basis, but the
    // default default should always be the zeroed-out value.
    memset(map->vals, 0, val_size);
    map->keys += key_size;
    map->vals += val_size;

    map->num_buckets = INITIAL_NUM_BUCKETS;
    map->buckets     = New(map->num_buckets, Hash_bucket, context);

    return map;
}

static void grow_map_if_needed(void *map)
{
    Alias_map *m = map;

    if (m->count >= m->limit-1) {
        // We're out of room in the key/value arrays.
        m->keys -= m->key_size;
        m->vals -= m->val_size;

        m->keys = resize(m->keys, 2*m->limit, m->key_size, m->context);
        m->vals = resize(m->vals, 2*m->limit, m->val_size, m->context);

        m->keys += m->key_size;
        m->vals += m->val_size;

        m->limit *= 2;
    }

    if (m->count >= m->num_buckets/4*3) {
        // More than 3/4 of the buckets are used.
        Hash_bucket *new_buckets = New(2*m->num_buckets, Hash_bucket, m->context);

        for (s64 old_i = 0; old_i < m->num_buckets; old_i++) {
            if (!m->buckets[old_i].hash)  continue;

            s64 new_i = m->buckets[old_i].hash % (2*m->num_buckets);
            while (true) {
                if (!new_buckets[new_i].hash) {
                    new_buckets[new_i] = m->buckets[old_i];
                    break;
                }
                new_i -= 1;
                if (new_i < 0)  new_i += 2*m->num_buckets;
            }
        }
        dealloc(m->buckets, m->context);
        m->buckets = new_buckets;
        m->num_buckets *= 2;
    }
}

static char *copy_string(char *source, Memory_context *context)
{
    int length = strlen(source);
    char *copy = alloc(length+1, sizeof(char), context);
    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

s64 set_key(void *map)
// Assume the key to set is stored in map->keys[-1]. Add the key to the map's hash table if it
// wasn't already there and return the key's index in the map->keys array.
{
    Alias_map *m = map;

    assert(m->context);
    assert(m->count < m->num_buckets); // Assume empty buckets exist so the while loops below aren't infinite loops.

    grow_map_if_needed(m);

    if (m->string_mode) {
        // The key is a C-style string.
        char *key = ((char **)m->keys)[-1];
        u64  hash = hash_string(key);
        s64 bucket_index = hash % m->num_buckets;
        while (true) {
            if (!m->buckets[bucket_index].hash) {
                // The bucket is empty. We'll take it.
                s64 kv_index = m->count;
                ((char **)m->keys)[kv_index] = copy_string(key, m->context);
                m->buckets[bucket_index].hash  = hash;
                m->buckets[bucket_index].index = kv_index;
                m->count += 1;
                return kv_index;
            }
            if (m->buckets[bucket_index].hash == hash) {
                // The hashes match. Make sure the keys match.
                s64 kv_index = m->buckets[bucket_index].index;
                if (!strcmp(key, ((char **)m->keys)[kv_index]))  return kv_index;
            }
            bucket_index -= 1;
            if (bucket_index < 0)  bucket_index += m->num_buckets;
        }
    } else {
        // The key is binary.
        void *key = (char *)m->keys - m->key_size; // key = m->keys[-1]
        u64  hash = hash_bytes(key, m->key_size);
        s64 bucket_index = hash % m->num_buckets;
        while (true) {
            if (!m->buckets[bucket_index].hash) {
                // The bucket is empty. We'll take it.
                s64 kv_index = m->count;
                memcpy(m->keys + kv_index*m->key_size, key, m->key_size);
                m->buckets[bucket_index].hash  = hash;
                m->buckets[bucket_index].index = kv_index;
                m->count += 1;
                return kv_index;
            }
            if (m->buckets[bucket_index].hash == hash) {
                // The hashes match. Make sure the keys match.
                s64 kv_index = m->buckets[bucket_index].index;
                if (!memcmp(key, (char *)m->keys + kv_index*m->key_size, m->key_size))  return kv_index;
            }
            bucket_index -= 1;
            if (bucket_index < 0)  bucket_index += m->num_buckets;
        }
    }
}

s64 get_bucket_index(void *map)
{
    Alias_map *m = map;

    assert(m->context);

    if (m->string_mode) {
        // The key is a C-style string.
        char *key = ((char **)m->keys)[-1];
        u64  hash = hash_string(key);
        s64 bucket_index = hash % m->num_buckets;
        while (true) {
            if (!m->buckets[bucket_index].hash)  return -1;

            if (m->buckets[bucket_index].hash == hash) {
                // The hashes match. Make sure the keys match.
                s64 kv_index = m->buckets[bucket_index].index;
                if (!strcmp(key, ((char **)m->keys)[kv_index]))  return bucket_index;
            }
            bucket_index -= 1;
            if (bucket_index < 0)  bucket_index = m->num_buckets-1; // |Cleanup: += for consistency?
        }
    } else {
        // The key is binary.
        void *key = (char *)m->keys - m->key_size; // key = m->keys[-1]
        u64  hash = hash_bytes(key, m->key_size);
        s64 bucket_index = hash % m->num_buckets;
        while (true) {
            if (!m->buckets[bucket_index].hash)  return -1;

            if (m->buckets[bucket_index].hash == hash) {
                // The hashes match. Make sure the keys match.
                s64 kv_index = m->buckets[bucket_index].index;
                if (!memcmp(key, (char *)m->keys + kv_index*m->key_size, m->key_size))  return bucket_index;
            }
            bucket_index -= 1;
            if (bucket_index < 0)  bucket_index = m->num_buckets-1; // |Cleanup: += for consistency?

        }
    }
}

bool delete_key(void *map)
// Return true if the key existed.
{
    s64 bucket_index = get_bucket_index(map);
    if (bucket_index < 0)  return false;

    Alias_map *m = map;

    s64 kv_index = m->buckets[bucket_index].index;

    // Delete the bucket. This is algorithm 6.4R from Knuth volume 3. Note that the errata for the
    // second edition of this book correct a significant bug in this algorithm. Step R4 should end
    // with "return to step R1", not "return to step R2".
    {
        s64 i = bucket_index;
        while (true) {
            m->buckets[i] = (Hash_bucket){0};

            s64 j = i;
            while (true) {
                i -= 1;
                if (i < 0)  i += m->num_buckets;

                if (!m->buckets[i].hash)  goto bucket_deleted;

                s64 r = m->buckets[i].hash % m->num_buckets;

                if (i <= r && r < j)  continue;
                if (r < j && j < i)   continue;
                if (j < i && i <= r)  continue;

                m->buckets[j] = m->buckets[i];
                break;
            }
        }
    }
bucket_deleted:

    // Delete the key and value.
    {
        // If it's a string-mode map, delete the copy we made of the key.
        if (m->string_mode)  dealloc(((char **)m->keys)[kv_index], m->context);

        if (kv_index < m->count-1) {
            // Copy the final kv pair into the places of the pair we're deleting.
            memcpy(m->keys+m->key_size*kv_index, m->keys+m->key_size*(m->count-1), m->key_size);
            memcpy(m->vals+m->val_size*kv_index, m->vals+m->val_size*(m->count-1), m->val_size);

            // Update the hash table with the new index of the pair that we moved.
            u64 hash = (m->string_mode)
                ? hash_string(((char **)m->keys)[m->count-1])
                : hash_bytes(m->keys+m->key_size*(m->count-1), m->key_size);

            s64 bucket_index = hash % m->num_buckets;
            while (true) {
                if (m->buckets[bucket_index].index == m->count-1) {
                    m->buckets[bucket_index].index = kv_index;
                    break;
                }
                assert(m->buckets[bucket_index].hash);
                bucket_index -= 1;
                if (bucket_index < 0)  bucket_index += m->num_buckets;
            }
        }
        // Delete the final kv pair.
        memset(m->keys+m->key_size*(m->count-1), 0, m->key_size);
        memset(m->vals+m->val_size*(m->count-1), 0, m->val_size);
    }

    m->count -= 1;

    return true;
}
