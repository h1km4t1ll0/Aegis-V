int write(int fd, const void *buf, unsigned long n);

int main(void)
{
    write(1, "init\n", 5);
    return 0;
}
