/* "tcc -ar" on Kronos: kmain builds arlink-lib.c into libarlink.a with
   /bin/tcc7b -ar, then links this file against it. The ar header's size
   fields were "%-10d", which Kronos libc.c printed right-aligned; the
   linker cut their last digit, read no symbols and left these references
   unresolved. */
int puts(const char *);
int arlink_twice(int v);
extern int arlink_seven;

int main(void)
{
    if (arlink_twice(arlink_seven) != 14)
        return 1;
    puts("arlink ok");
    return 0;
}
