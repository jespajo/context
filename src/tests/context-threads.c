#include "../array.h"

typedef Array(Memory_context *)  Memory_context_array;

float randf()
{
    return (float)rand()/RAND_MAX;
}

void *random_alloc(Memory_context *context)
{
    if (!context->used_count)  return NULL;

    s64 index = rand() % context->used_count;

    Memory_block *block = &context->used_blocks[index];

    if (!block->size)  return NULL;

    return block->data;
}

void *thread_routine(void *arg)
{
    int num_loops = 1000;

    Memory_context_array *contexts = arg;

    for (int loop = 0; loop < num_loops; loop++) {
        int context_index = randf()*contexts->count;
        assert(0 <= context_index && context_index < contexts->count);

        Memory_context *ctx = contexts->data[context_index];

        // Make a random number of allocations.
        while (randf() < 0.9) {
            s64   limit = rand() % 100 + 1;
            u64   unit  = rand() % 16 + 1;
            void *data  = alloc(limit, unit, ctx);

            memset(data, -1, limit*unit);
        }

        // Make a random number of deallocations.
        while (randf() < 0.5) {
            pthread_mutex_lock(&ctx->mutex);

            u8 *rand_data = random_alloc(ctx);

            bool can_delete = false;

            if (rand_data) {
                // |Dodgy. To make sure the allocation isn't being used, we check whether the first byte is -1
                // (since this is what we memset our pointless allocations to). Then, before we release the lock,
                // we set the first byte to 0 so that other threads won't deallocate it before this one can.
                int negative_one = -1;
                can_delete = !memcmp(rand_data, &negative_one, 1);
                if (can_delete)  rand_data[0] = 0;
            }

            pthread_mutex_unlock(&ctx->mutex);

            if (can_delete)  dealloc(rand_data, ctx);
        }

        check_context_integrity(ctx);
    }

    return NULL;
}

int main()
{
    int num_threads  = 12;
    int num_contexts = 10;

    Memory_context *top_context = new_context(NULL);

    Memory_context_array contexts = {.context = top_context};
    for (int i = 0; i < num_contexts; i++) {
        *Add(&contexts) = new_context(top_context);
    }

    Array(pthread_t) threads = {.context = top_context};
    for (int i = 0; i < num_threads; i++) {
        int r = pthread_create(Add(&threads), NULL, thread_routine, &contexts);
        if (r) {
            Fatal("Failed to create a thread.\n");
        }
    }

    for (int i = 0; i < num_threads; i++) {
        int r = pthread_join(threads.data[i], NULL);
        if (r) {
            Fatal("Failed to join a thread.\n");
        }
    }

    free_context(top_context);

    return 0;
}
