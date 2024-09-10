#include <time.h>

#include "../context.h"

float randf()
{
    return (float)rand()/(float)RAND_MAX;
}

void *random_alloc(Memory_context *context)
{
    if (!context->used_count)  return NULL;

    s64 index = rand() % context->used_count;

    Memory_block *block = &context->used_blocks[index];

    if (!block->size)  return NULL;

    return block->data;
}

int get_depth(Memory_context *context)
{
    int depth = 0;

    while (context->parent) {
        depth  += 1;
        context = context->parent;
    }

    return depth;
}

int main()
{
    //
    // Stress test context.c.
    //
    //int seed = time(NULL);
    int seed = 2;
    printf("seed: %d\n", seed);
    fflush(stdout);
    srand(seed);

    Memory_context *ctx = new_context(NULL);

    s64 num_loops = 1<<13;

    for (s64 loop = 0; loop < num_loops; loop++) {
        // Make a random number of allocations.
        while (randf() < 0.9) {
            s64   limit = rand() % 100 + 1;
            u64   unit  = rand() % 16 + 1;
            void *data  = alloc(limit, unit, ctx);

            memset(data, -1, limit*unit);

            check_context_integrity(ctx);
        }

        // Maybe free something random.
        if (randf() < 0.2) {
            void *data = random_alloc(ctx);
            if (data)  dealloc(data, ctx);

            check_context_integrity(ctx);
        }

        // Maybe resize something random.
        if (randf() < 0.3) {
            void *data = random_alloc(ctx);
            if (data) {
                s64   limit   = rand() % 100 + 1;
                u64   unit    = rand() % 16 + 1;
                void *resized = resize(data, limit, unit, ctx);

                memset(resized, -3, limit*unit);

                check_context_integrity(ctx);
            }
        }

        // Maybe switch to the parent context.
        if (randf() < 0.05) {
            Memory_context *parent = ctx->parent;
            if (parent) {
                // Maybe also free the child context.
                if (randf() < 0.02) {
                    for (s64 i = 0; i < ctx->buffer_count; i++) {
                        memset(ctx->buffers[i].data, -4, ctx->buffers[i].size);
                    }
                    free_context(ctx);
                }
                ctx = parent;
            }
        }

        // Maybe switch to a new child context.
        if (randf() < 0.05) {
            if (get_depth(ctx) < 6)  ctx = new_context(ctx);
        }

        // Maybe reset the current context.
        if (randf() < 0.01)  reset_context(ctx);
    }

    // Make sure we free the top context.
    while (ctx->parent)  ctx = ctx->parent;
    free_context(ctx);

    return 0;
}
