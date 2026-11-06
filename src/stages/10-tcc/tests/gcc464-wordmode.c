/* gcc464: __mode__(__word__) is the register width, and a mode
   attribute after the declarator name has to change the typedef.
   libcpp's line scanner uses this type.  A 4-byte word misses
   newlines that sit past the first byte of a word. */
typedef unsigned int word_type __attribute__((__mode__(__word__)));

int main(void)
{
    if (sizeof(word_type) != sizeof(void *))
        return 1;
    return 0;
}
