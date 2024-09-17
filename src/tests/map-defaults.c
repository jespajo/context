#include "../map.h"

int main()
{
    Memory_context *ctx = new_context(NULL);

    Dict(int) *dict = NewDict(dict, ctx);

    assert(!IsSet(dict, "one"));

    *Set(dict, "one") = 1;

    assert(IsSet(dict, "one"));
    assert(*Get(dict, "one") == 1);
    assert(*Get(dict, "two") == 0);
    assert(!IsSet(dict, "two"));

    SetDefault(dict, -1);

    assert(!IsSet(dict, "two"));
    assert(*Get(dict, "two") == -1);

    *Set(dict, "two") = 2;

    assert(IsSet(dict, "two"));
    assert(*Get(dict, "two") == 2);

    *Set(dict, "three") = 3;
    *Set(dict, "four")  = 4;
    *Set(dict, "five")  = 5;
    *Set(dict, "six")   = 6;
    *Set(dict, "seven") = 7;
    *Set(dict, "eight") = 8;
    *Set(dict, "nine")  = 9;

    assert(!IsSet(dict, "ten"));
    assert(*Get(dict, "ten") == -1);

    SetDefault(dict, -2);

    assert(!IsSet(dict, "ten"));
    assert(*Get(dict, "ten") == -2);

    Delete(dict, "two");

    assert(!IsSet(dict, "two"));
    assert(*Get(dict, "two") == -2);

    free_context(ctx);
    return 0;
}
