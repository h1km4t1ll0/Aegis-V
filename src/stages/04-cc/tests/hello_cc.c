unsigned write(FILE* f, char* buffer, unsigned count)
{
	asm("rd_a0 rs1_fp !-8 ld"
	    "rd_a1 rs1_fp !-16 ld"
	    "rd_a2 rs1_fp !-24 ld"
	    "rd_a7 !64 addi"
	    "ecall");
}

int main()
{
	write(1, "hello from C\n", 13);
	return 0;
}
