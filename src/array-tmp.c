/*

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

    assert(context);

    if (!data) {
        // The array needs to be initialised.
        assert(*limit == 0 && count == 0);

        *limit = INITIAL_LIMIT;
        data   = alloc(*limit, unit_size, context);
    } else if (count >= *limit) {
        // The array needs to be resized.
        assert(count == *limit);

        // Make sure we only use this function for arrays that should increase in powers of two. This assert will trip
        // if we use array_reserve() to reserve a non-power-of-two number of bytes for an array and then exceed this
        // limit with Add(). In this case, just round up the array_reserve() argument to a power of two.
        assert(is_power_of_two(*limit));

        *limit *= 2;
        data    = resize(data, *limit, unit_size, context);
    }

    return data;
}

*/
