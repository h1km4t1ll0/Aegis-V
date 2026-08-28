/* M2-Planet on RV64 emits global arrays as a pointer word of %0 plus
 * :GLOBAL_STORAGE_name. hex2 never relocates %0, so heap[i] stores to NULL.
 * Allocate at runtime instead. int is 8 bytes on this M2. */

void pnut_alloc_bss(void)
{
	int i;
	fd = -1;
	string_pool = malloc(STRING_POOL_SIZE);
	heap = malloc(HEAP_SIZE * 8);
	code = malloc(CODE_SIZE * 8);
#ifdef LABELS_ARR_SIZE
	labels = malloc(LABELS_ARR_SIZE * 8);
#endif
	include_stack = malloc(INCLUDE_STACK_DEPTH * INCLUDE_ENTRY_SIZE * 8);
	if_macro_stack = malloc(IFDEF_DEPTH_MAX * 8);
	macro_stack = malloc(MACRO_RECURSION_MAX * 8);
	write_buf = malloc(8);
	read_buf = malloc(8);
#ifdef SUPPORT_64_BIT_LITERALS
	val_32 = malloc(16);
#endif
	if(string_pool == 0 || heap == 0 || code == 0
#ifdef LABELS_ARR_SIZE
		|| labels == 0
#endif
		|| include_stack == 0 || if_macro_stack == 0 || macro_stack == 0
		|| write_buf == 0 || read_buf == 0)
	{
		fputs("pnut: malloc failed\n", stdout);
		exit(1);
	}
#ifdef LABELS_ARR_SIZE
	i = 0;
	while(i < LABELS_ARR_SIZE)
	{
		labels[i] = 0;
		i = i + 1;
	}
#endif
	i = 0;
	while(i < IFDEF_DEPTH_MAX)
	{
		if_macro_stack[i] = 0;
		i = i + 1;
	}
}
