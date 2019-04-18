#include <stdio.h>

#include "bitmap.h"

int bitmap_get(void *bm, int ii)
{
    int *ptr = (int *)bm + ii;
    return (*ptr);
}
void bitmap_put(void *bm, int ii, int vv)
{
    int *ptr = (int *)bm + ii;
    (*ptr) = vv;
}
void bitmap_print(void *bm, int size)
{
    for (int ii = 0; ii < size; ++ii)
    {
        int *ptr = (int *)bm + ii;
        printf("%d ", (*ptr));
    }
}