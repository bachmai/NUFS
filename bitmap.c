int bitmap_get(void* bm, int ii) 
{
    return *((int*)bm + (ii/32)) & (1 << (ii%32));
}

void bitmap_put(void* bm, int ii, int vv) 
{
    // TODO
}

void bitmap_print(void* bm, int size) 
{
    // TODO
}