enum
{
	max_string = 4096,
	KNIGHT = 0,
	X86 = 3,
	AMD64 = 0x3E,
	ARMV7L = 0x28,
	AARM64 = 0xB7,
	PPC64LE = 0x15,
	RISCV32 = 0xF3,
	RISCV64 = 0x100F3,
	HEX = 16,
	OCTAL = 8,
	BINARY = 2
};

struct input_files
{
	struct input_files* next;
	char* filename;
};

struct entry
{
	struct entry* next;
	unsigned target;
	char* name;
};
