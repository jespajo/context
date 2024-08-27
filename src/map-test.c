#include "map.h"

int main()
{
    Memory_context *ctx = new_context(NULL);

    Dict(int) *dict = NewDict(dict, ctx);

    *Set(dict, "one") = 1;
    *Set(dict, "two") = 2;




    free_context(ctx);
    return 0;
}
