//|Todo: Some kind of visualisation would be really helpful.

//|Speed: At the moment we frequently operate on the arrays of blocks by deleting a block with delete_block() and then adding
// blocks with add_block(). Each of these functions leaves the array sorted, so if we delete the first block in the array, it
// will shift all subsequent blocks to the left, and if we then insert a new block at the start of the array, it will shift
// everything back to the right. There is room for improvement here. Solving this may involve some kind of transaction-based
// approach---you'd store up the changes you want to make (delete this block, insert two before that one) and then make all the
// changes at once. Maybe if we had these kinds of transactions we could even make memory contexts threadsafe---though threads
// sharing a context is probably a bad idea anyway because contention would make it slow.

#include "context.h"

void *double_if_needed(void *data, s64 *limit, s64 count, u64 unit_size, Memory_context *context)
// Make sure there's room for at least one more item in the array. If reallocation occurs, modify *limit and
// return a pointer to the new data. Otherwise return data.
//
// You may ask, if this function modifies *limit, why does it rely on its caller to assign the return value
// to data themselves? Couldn't we just make make the first parameter `void **data` and get the function to
// modify *data as well? No. The reason is that the compiler lets us implicitly cast e.g. `int *` to `void *`,
// but not `int **` to `void **`.
{
    s64 INITIAL_LIMIT = 4; // If the array is unitialised, how many units to make room for in the first allocation.

    if (!data) {
        // The array needs to be initialised.
        assert(*limit == 0 && count == 0);

        *limit = INITIAL_LIMIT;
        data   = alloc(context, *limit, unit_size);
    } else if (count >= *limit) {
        // The array needs to be resized.
        assert(count == *limit);

        // Make sure we only use this function for arrays that should increase in powers of two. If this assert trips,
        // it probably means we used array_reserve() to reserve a non-power-of-two number of bytes for an array and then
        // exceeded this limit with Add(). In this case, round up the array_reserve() argument to a power of two.
        assert(is_power_of_two(*limit));

        *limit *= 2;
        data    = resize(context, data, *limit, unit_size);
    }

    return data;
}

static s64 get_free_block_index(Memory_context *context, u64 size, u8 *data)
// Return the index of the block if it exists or the index where it would be inserted.
{
    s64 i = 0;
    s64 j = context->free_count-1;

    while (i <= j) {
        s64 mid = (i + j)/2;
        Memory_block *block = &context->free_blocks[mid];

        s64 cmp = size - block->size;
        if (!cmp) {
            cmp = data - block->data;
            if (!cmp)  return mid;
        }

        if (cmp < 0)  j = mid-1;
        else          i = mid+1;
    }

    return i;
}

static Memory_block *find_free_block(Memory_context *context, u64 size, u8 *data)
{
    s64 index = get_free_block_index(context, size, data);

    if (index < context->free_count) {
        Memory_block *block = &context->free_blocks[index];

        if (block->data == data && block->size == size)  return block;
    }

    return NULL;
}

static s64 get_used_block_index(Memory_context *context, u8 *data)
// Return the index of the block if it exists or the index where it would be inserted.
{
    s64 i = 0;
    s64 j = context->used_count-1;

    while (i <= j) {
        s64 mid = (i + j)/2;
        Memory_block *block = &context->used_blocks[mid];

        s64 cmp = data - block->data;
        if (!cmp) {
            // If it's a sentinel, return the next index.
            if (!block->size) {
                mid += 1;
                // There may be two sentinels with the same address if the context has two contiguous buffers:
                // one for the end of the first buffer and for the start of the second. So maybe skip one more.
                if (mid < context->used_count) {
                    block = &context->used_blocks[mid];
                    if (block->data == data && !block->size)  mid += 1;
                }
            }
            return mid;
        }

        if (cmp < 0)  j = mid-1;
        else          i = mid+1;
    }

    return i;
}

static Memory_block *find_used_block(Memory_context *context, u8 *data)
{
    s64 index = get_used_block_index(context, data);

    if (index < context->used_count) {
        Memory_block *block = &context->used_blocks[index];

        if (block->data == data)  return block;
    }

    return NULL;
}

static bool in_range(void *zero, void *x, void *zero_plus_count)
{
    return (u64)zero <= (u64)x && (u64)x < (u64)zero_plus_count;
}

#ifndef DEBUG_MEMORY_CONTEXT
#define assert_context_makes_sense(C)  ((void)0)
#else
static bool are_in_free_order(Memory_block *blocks, s64 count)
{
    for (s64 i = 0; i < count-1; i++) {
        if (blocks[i].size > blocks[i+1].size)   return false;

        if (blocks[i].size == blocks[i+1].size) {
            if (blocks[i].data > blocks[i+1].data)  return false;
        }
    }
    return true;
}

static bool are_in_used_order(Memory_block *blocks, s64 count)
{
    for (s64 i = 0; i < count-1; i++) {
        if (blocks[i].data > blocks[i+1].data)  return false;

        if (blocks[i].data == blocks[i+1].data) {
            if (blocks[i].size)  return false;
        }
    }
    return true;
}

static void assert_context_makes_sense(Memory_context *context)
{
    Memory_context *c = context;

    assert(are_in_free_order(c->free_blocks, c->free_count));
    assert(are_in_used_order(c->used_blocks, c->used_count));

    s64 num_free = 0;
    s64 num_used = 0;

    // For each buffer, enumerate all blocks to make sure they look right.
    for (s64 buffer_index = 0; buffer_index < c->buffer_count; buffer_index++) {
        Memory_block *buffer = &c->buffers[buffer_index];
        u8 *buffer_end = buffer->data + buffer->size;

        u8 *data = buffer->data;

        Memory_block *last_used; {
            s64 index = get_used_block_index(c, data);
            last_used = &c->used_blocks[index-1];
        }
        assert(last_used->data == buffer->data);
        assert(last_used->size == 0);

        num_used += 1;

        while (data < buffer_end) {
            Memory_block *used_block = find_used_block(c, data);
            if (used_block) {
                assert(used_block->size);

                data     += used_block->size;
                num_used += 1;
                last_used = used_block;
            } else {
                s64 last_used_index = last_used - c->used_blocks;
                assert(last_used_index < c->used_count-1);

                u8 *next_data = (last_used+1)->data;
                assert(next_data <= buffer_end);

                s64 free_size = next_data - data;
                Memory_block *free_block = find_free_block(c, free_size, data);
                assert(free_block);

                data     += free_block->size;
                num_free += 1;
            }
        }

        assert(data == buffer_end);

        last_used += 1;
        assert(last_used - c->used_blocks < c->used_count);

        assert(last_used->data == buffer_end);
        assert(last_used->size == 0);

        num_used += 1;
    }

    assert(num_free == c->free_count);
    assert(num_used == c->used_count);
}
#endif // DEBUG_MEMORY_CONTEXT

static bool is_sentinel(Memory_context *context, u8 *data, u64 size)
// Return true if the given pointer and size make sense as a sentinel for the given Memory_context
// (i.e. they would mark the start or end of one of the context's buffers).
{
    if (size)  return false;

    for (s64 i = 0; i < context->buffer_count; i++) {
        Memory_block *buffer = &context->buffers[i];

        if (data == buffer->data)                 return true;
        if (data == buffer->data + buffer->size)  return true;
    }

    return false;
}

static Memory_block *add_block(Memory_context *context, Memory_block **blocks, void *data, u64 size)
// Add a block with the specified pointer and size to an array of Memory_blocks, maintaining the array's order.
{
    Memory_context *c = context;

    assert(blocks == &c->free_blocks || blocks == &c->used_blocks);

    bool is_used = (blocks == &c->used_blocks);

    assert(data);
    assert(size || (is_used && is_sentinel(c, data, size)));

    s64 *count = is_used ? &c->used_count : &c->free_count;
    s64 *limit = is_used ? &c->used_limit : &c->free_limit;

    *blocks = double_if_needed(*blocks, limit, *count, sizeof(**blocks), c->parent);

    s64 insert_index = is_used ? get_used_block_index(c, data) : get_free_block_index(c, size, data);

    // Make room by shifting everything after block_index right one.
    for (s64 i = *count; i > insert_index; i--)  (*blocks)[i] = (*blocks)[i-1];

    (*blocks)[insert_index] = (Memory_block){.data=data, .size=size};
    *count += 1;

    return &(*blocks)[insert_index];
}

static Memory_block *grow_context(Memory_context *context, u64 size)
// Add a new buffer of at least size bytes to a context. Return the associated free block.
{
    u64 FIRST_BUFFER_SIZE = 8192;

    Memory_context *c = context;

    Memory_block buffer = {0};

    // Our idea here is to double the size of each additional buffer that we add to a context.
    // We think this will help with fragmentation (particularly with child contexts) and reduce
    // the number of allocations from the OS.
    if (!c->buffer_count)  buffer.size = FIRST_BUFFER_SIZE;
    else                   buffer.size = 2 * c->buffers[c->buffer_count-1].size;

    // Keep doubling until we know we have room for an allocation of length `size`.
    while (buffer.size < size)  buffer.size *= 2;

    buffer.data = alloc(c->parent, 1, buffer.size);

    c->buffers = double_if_needed(c->buffers, &c->buffer_limit, c->buffer_count, sizeof(*c->buffers), c->parent);

    c->buffers[c->buffer_count] = buffer;
    c->buffer_count += 1;

    // Create sentinel used blocks at the beginning and end of the buffer.
    add_block(c, &c->used_blocks, buffer.data,               0);
    add_block(c, &c->used_blocks, buffer.data + buffer.size, 0);

    Memory_block *free_block = add_block(c, &c->free_blocks, buffer.data, buffer.size);

    assert_context_makes_sense(c);

    return free_block;
}

static void delete_block(Memory_block *blocks, s64 *count, Memory_block *block)
// Remove a block from an array of blocks. Decrement *count.
{
    s64 index = block - blocks;
    assert(0 <= index && index < *count);

    // Move subsequent blocks left one, then delete the final block.
    for (s64 i = index+1; i < *count; i++)  blocks[i-1] = blocks[i];

    blocks[*count-1] = (Memory_block){0};

    *count -= 1;
}

static u64 get_alignment(u64 unit_size)
{
    u64 max_align = 16;
    s64 alignment = (unit_size < max_align) ? round_up_pow2(unit_size) : max_align;

    return alignment;
}

static u64 get_padding(u8 *data, u64 alignment)
// Get alignment padding size in bytes.
{
    u64 gap     = (u64)data % alignment;
    u64 padding = (gap) ? alignment - gap : 0;

    return padding;
}

static Memory_block *alloc_block(Memory_context *context, Memory_block *free_block, u64 size, u64 alignment)
// Return a pointer to the newly used block on success. Return NULL if there's not room due to alignment.
{
    Memory_context *c = context;

    assert(in_range(c->free_blocks, free_block, c->free_blocks+c->free_count));
    assert(free_block->size >= size); // This is not necessary (we return NULL in this case) but otherwise why are you calling this function?

    u64 padding = get_padding(free_block->data, alignment);

    if (free_block->size < padding)         return NULL;
    if (free_block->size - padding < size)  return NULL;

    u64 remaining = free_block->size - padding - size;

    // |Speed: For now we're just going to add and delete the relevant blocks one at a time.

    u8 *free_data = free_block->data;

    delete_block(c->free_blocks, &c->free_count, free_block);

    if (padding)  add_block(c, &c->free_blocks, free_data, padding);

    Memory_block *used_block = add_block(c, &c->used_blocks, free_data+padding, size);

    if (remaining) {
        u8 *next_free = used_block->data + used_block->size;
        add_block(c, &c->free_blocks, next_free, remaining);
    }

    assert_context_makes_sense(c);

    return used_block;
}

static Memory_block *dealloc_block(Memory_context *context, Memory_block *used_block)
// Return coalesced free block.
{
    Memory_context *c = context;

    assert(in_range(c->used_blocks, used_block, c->used_blocks+c->used_count));
    assert(used_block->size);

    // |Speed: For now we're just going to add and delete the relevant blocks one at a time.

    u8 *freed_data = used_block->data;
    u64 freed_size = used_block->size;
    s64 used_index = used_block - c->used_blocks;

#ifdef DEBUG_MEMORY_CONTEXT
    memset(freed_data, -1, freed_size);//|Temporary: Let's be more thoughtful about this.
#endif

    // These asserts should always be true due to the presence of sentinels. If they are untrue
    // the pointer arithmetic used below to check the neighbouring used blocks is invalid.
    assert(used_index > 0);
    assert(used_index < c->used_count-1);

    // See if we should coalesce with the left neighbour.
    {
        Memory_block *prev_used = used_block - 1;
        u8 *prev_used_end = prev_used->data + prev_used->size;
        s64 distance = used_block->data - prev_used_end;
        if (distance) {
            Memory_block *left = find_free_block(c, distance, prev_used_end);
            freed_data -= left->size;
            freed_size += left->size;
            delete_block(c->free_blocks, &c->free_count, left);
        }
    }
    // See if we should coalesce with the right neighbour.
    {
        Memory_block *next_used = used_block + 1;
        u8 *used_block_end = used_block->data + used_block->size;
        s64 distance = next_used->data - used_block_end;
        if (distance) {
            Memory_block *right = find_free_block(c, distance, used_block_end);
            freed_size += right->size;
            delete_block(c->free_blocks, &c->free_count, right);
        }
    }

    delete_block(c->used_blocks, &c->used_count, used_block);

    Memory_block *freed_block = add_block(c, &c->free_blocks, freed_data, freed_size);

    assert_context_makes_sense(c);

    return freed_block;
}

static Memory_block *resize_block(Memory_context *context, Memory_block *used_block, u64 new_size)
// Return the resized block if success, or NULL if there isn't room in a contiguous free block; in that case the caller will have to call alloc_block and dealloc_block.
{
    Memory_context *c = context;

    assert(in_range(c->used_blocks+1, used_block, c->used_blocks+c->used_count-1));

    // Don't bother shrinking. (Maybe one day.)
    if (new_size <= used_block->size)  return used_block;

    Memory_block *next_used = used_block + 1;

    u8 *end_of_used_block = used_block->data + used_block->size;
    u64 size_avail_after  = next_used->data - end_of_used_block;

    // Return NULL if there's not enough room after the block.
    // |Todo: Maybe also check if there's room *before* the used block. If so, it would probably be
    // better than telling the caller to reallocate. We'd have to pass the unit size to this function
    // or just be super conservative about alignment.
    if (used_block->size + size_avail_after < new_size)  return NULL;

    // We can expand this block.
    Memory_block *free_neighbour = find_free_block(c, size_avail_after, end_of_used_block);
    assert(free_neighbour);

    u64 extra_needed    = new_size - used_block->size;
    u64 remaining_after = free_neighbour->size - extra_needed;

    used_block->size = new_size;
    u8 *new_end_of_used_block = used_block->data + new_size;

    delete_block(c->free_blocks, &c->free_count, free_neighbour);

    if (remaining_after)  add_block(c, &c->free_blocks, new_end_of_used_block, remaining_after);

    assert_context_makes_sense(c);

    return used_block;
}

void *alloc(Memory_context *context, s64 count, u64 unit_size)
{
    Memory_context *c = context;

    assert(count);
    assert(unit_size);

    u64 size = count * unit_size;

    if (!context) {
        void *memory = malloc(size);
        if (!memory)  Fatal("malloc failed.");
        return memory;
    }

    u64 alignment = get_alignment(unit_size);

    // See if there's an already-free block of the right size.
    for (s64 i = get_free_block_index(c, size, NULL); i < c->free_count; i++) {
        Memory_block *free_block = &c->free_blocks[i];
        Memory_block *used_block = alloc_block(c, free_block, size, alignment);

        if (used_block)  return used_block->data;
    }

    // We weren't able to find a block big enough in the free list.
    // We need to add a new buffer to the context.
    Memory_block *free_block = grow_context(context, size);

    return alloc_block(c, free_block, size, alignment)->data;
}

void *zero_alloc(Memory_context *context, s64 count, u64 unit_size)
{
    void *data = alloc(context, count, unit_size);

    memset(data, 0, count*unit_size);

    return data;
}

void dealloc(Memory_context *context, void *data)
{
    assert(data);

    if (!context) {
        free(data);
        return;
    }

    Memory_block *used_block = find_used_block(context, data);
    assert(used_block);

    dealloc_block(context, used_block);
}

void *resize(Memory_context *context, void *data, s64 new_limit, u64 unit_size)
{
    Memory_context *c = context;

    assert(data);
    assert(new_limit);
    assert(unit_size);

    u64 new_size = new_limit * unit_size;

    if (!context) {
        void *new_data = realloc(data, new_size);
        if (!new_data)  Fatal("realloc failed.");
        return new_data;
    }

    Memory_block *used_block = find_used_block(c, data);
    assert(used_block);

    Memory_block *resized = resize_block(context, used_block, new_size);
    if (resized)  return resized->data;

    // We can't resize the block in place. We'll have to move it.

    s64 old_index = used_block - c->used_blocks;

    void *new_data = alloc(context, new_limit, unit_size);

    // `alloc` may have made an unknown number of allocations or reallocations. Which means the used
    // block's index might have changed and the whole array of used blocks might have moved. We need
    // to find the old used block in this case so we can deallocate it.
    used_block = &c->used_blocks[old_index];
    if (used_block->data < (u8 *)data || !used_block->size) {
        do used_block += 1;  while (data != used_block->data || !used_block->size);
    } else if (used_block->data > (u8 *)data) {
        do used_block -= 1;  while (data != used_block->data);
    }

    u64 copy_size = Min(used_block->size, new_size);
    memcpy(new_data, data, copy_size);

    dealloc_block(context, used_block);

    return new_data;
}

Memory_context *new_context(Memory_context *parent)
{
    Memory_context *context = New(Memory_context, parent);

    context->parent = parent;

    return context;
}

void free_context(Memory_context *context)
{
    Memory_context *c = context;

    // This automatically frees all child contexts because they all allocated from this parent.

    for (s64 i = 0; i < c->buffer_count; i++) {
        if (c->buffers[i].data)  dealloc(c->parent, c->buffers[i].data);
    }

    if (c->buffers)      dealloc(c->parent, c->buffers);
    if (c->free_blocks)  dealloc(c->parent, c->free_blocks);
    if (c->used_blocks)  dealloc(c->parent, c->used_blocks);

    dealloc(c->parent, c);
}

void reset_context(Memory_context *context)
{
    Memory_context *c = context;

    c->free_count = 0;
    c->used_count = 0;

    for (s64 i = 0; i < c->buffer_count; i++) {
        u8 *data = c->buffers[i].data;
        u64 size = c->buffers[i].size;

        // Add the sentinels.
        add_block(c, &c->used_blocks, data,      0);
        add_block(c, &c->used_blocks, data+size, 0);

        add_block(c, &c->free_blocks, data, size);
    }

    assert_context_makes_sense(c);
}

char *copy_string(char *source, Memory_context *context)
{
    int length = strlen(source);
    char *copy = alloc(context, length+1, sizeof(char));
    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}
