/* the other translation unit of nopic.c */
int nopic_var = 42;
long nopic_arr[1024];
int (*nopic_fp)(int);

int nopic_add(int x)
{
    return x + nopic_var + (int)nopic_arr[1000];
}
