#include "context.h"

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
    srand(1);

    Memory_context *ctx = new_context(NULL);

    s64 num_loops = 1<<16;

    for (s64 loop = 0; loop < num_loops; loop++) {
        // Make a random number of allocations.
        while (randf() < 0.9) {
            s64   limit = rand() % 100 + 1;
            u64   unit  = rand() % 16 + 1;
            void *data  = alloc(ctx, limit, unit);
        }

        // Maybe free something random.
        if (randf() < 0.3) {
            void *data = random_alloc(ctx);
            if (data)  dealloc(ctx, data);
        }

        // Maybe resize something random.
        if (randf() < 0.3) {
            void *data = random_alloc(ctx);
            if (data) {
                s64   limit   = rand() % 100 + 1;
                u64   unit    = rand() % 16 + 1;
                void *resized = resize(ctx, data, limit, unit);
            }
        }

        // Maybe switch to the parent context.
        if (randf() < 0.05) {
            Memory_context *parent = ctx->parent;
            if (parent) {
                // Maybe also free the child context.
                if (randf() < 0.1)  free_context(ctx);
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
