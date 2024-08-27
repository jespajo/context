#include "array.h"

#define StackArray(BUFFER) \
    {.data=(BUFFER), .limit=countof(BUFFER)}

int main()
{
    int buffer[100];
    int_array array = StackArray(buffer);

    *Add(&array) = 1;
    *Add(&array) = 1;
    *Add(&array) = 1;

    return 0;
}
