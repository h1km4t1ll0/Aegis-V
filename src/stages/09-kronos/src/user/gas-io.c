/* tcc-compiled glue: ET_REL emit and ET_REL→ET_EXEC link.
   Kept out of libc.c so init.elf does not need as/ld helpers. */

int open(const char *path, int flags, int mode);
long read(int fd, void *buf, unsigned long n);
long write(int fd, const void *buf, unsigned long n);
int close(int fd);
int strcmp(const char *a, const char *b);
int atoi(const char *s);
int puts(const char *s);
void elf_emit(int w);
void elf_flush(void);
void elf_reset(void);

#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512

static unsigned char obj_words[4096];
static int obj_nins;
static unsigned char obj_img[8192];
static char gas_src[2048];
static char gas_line[128];
static char *gas_tok[8];
static char labn[64][32];
static int labpc[64];
static int nlab;
static char obj_path[64];
static char undn[16][16];
static int undat[16];
static int nund;
static char defn[16][16];
static int defat[16];
static int ndef;
static unsigned char link_text[4096];
static int link_n;
static char lundn[32][16];
static int lundat[32];
static int lnund;
static char ldefn[32][16];
static int ldefat[32];
static int lndef;

static void wr16(unsigned char *p, int v)
{
    p[0] = (unsigned char)(v & 255);
    p[1] = (unsigned char)((v >> 8) & 255);
}

static void wr32(unsigned char *p, int v)
{
    p[0] = (unsigned char)(v & 255);
    p[1] = (unsigned char)((v >> 8) & 255);
    p[2] = (unsigned char)((v >> 16) & 255);
    p[3] = (unsigned char)((v >> 24) & 255);
}

static void wr64(unsigned char *p, int v)
{
    wr32(p, v);
    wr32(p + 4, 0);
}

static int rd16(unsigned char *p)
{
    return (int)p[0] | ((int)p[1] << 8);
}

static int rd32(unsigned char *p)
{
    return (int)p[0] | ((int)p[1] << 8) | ((int)p[2] << 16) | ((int)p[3] << 24);
}

void obj_emit(int w)
{
    unsigned char *p;
    if (obj_nins > 4092)
        return;
    p = obj_words + obj_nins;
    p[0] = (unsigned char)(w & 255);
    p[1] = (unsigned char)((w >> 8) & 255);
    p[2] = (unsigned char)((w >> 16) & 255);
    p[3] = (unsigned char)((w >> 24) & 255);
    obj_nins = obj_nins + 4;
}

static void obj_put(int b)
{
    if (obj_nins > 4095)
        return;
    obj_words[obj_nins] = (unsigned char)b;
    obj_nins = obj_nins + 1;
}

void obj_flush(void)
{
    unsigned char *sh;
    int i;
    int j;
    int fd;
    int n;

    i = 0;
    while (i < 8192) {
        obj_img[i] = 0;
        i = i + 1;
    }
    obj_img[0] = 0x7f;
    obj_img[1] = 'E';
    obj_img[2] = 'L';
    obj_img[3] = 'F';
    obj_img[4] = 2;
    obj_img[5] = 1;
    obj_img[6] = 1;
    wr16(obj_img + 16, 1);
    wr16(obj_img + 18, 243);
    wr32(obj_img + 20, 1);
    wr64(obj_img + 40, 64);
    wr16(obj_img + 52, 64);
    wr16(obj_img + 58, 64);
    wr16(obj_img + 60, 3);
    wr16(obj_img + 62, 2);
    sh = obj_img + 128;
    wr32(sh, 1);
    wr32(sh + 4, 1);
    wr64(sh + 8, 6);
    wr64(sh + 24, 0x120);
    wr64(sh + 32, obj_nins);
    wr64(sh + 48, 4);
    sh = obj_img + 192;
    wr32(sh, 7);
    wr32(sh + 4, 3);
    wr64(sh + 24, 0x100);
    wr64(sh + 32, 17);
    wr64(sh + 48, 1);
    obj_img[0x100] = 0;
    obj_img[0x101] = '.';
    obj_img[0x102] = 't';
    obj_img[0x103] = 'e';
    obj_img[0x104] = 'x';
    obj_img[0x105] = 't';
    obj_img[0x106] = 0;
    obj_img[0x107] = '.';
    obj_img[0x108] = 's';
    obj_img[0x109] = 'h';
    obj_img[0x10a] = 's';
    obj_img[0x10b] = 't';
    obj_img[0x10c] = 'r';
    obj_img[0x10d] = 't';
    obj_img[0x10e] = 'a';
    obj_img[0x10f] = 'b';
    obj_img[0x110] = 0;
    i = 0;
    while (i < obj_nins) {
        obj_img[0x120 + i] = obj_words[i];
        i = i + 1;
    }
    n = 0x120 + obj_nins;
    wr32(obj_img + n, 0x31434c52);
    wr32(obj_img + n + 4, nund);
    wr32(obj_img + n + 8, ndef);
    n = n + 12;
    i = 0;
    while (i < nund) {
        j = 0;
        while (j < 16) {
            obj_img[n + j] = (unsigned char)undn[i][j];
            j = j + 1;
        }
        wr32(obj_img + n + 16, undat[i]);
        n = n + 20;
        i = i + 1;
    }
    i = 0;
    while (i < ndef) {
        j = 0;
        while (j < 16) {
            obj_img[n + j] = (unsigned char)defn[i][j];
            j = j + 1;
        }
        wr32(obj_img + n + 16, defat[i]);
        n = n + 20;
        i = i + 1;
    }
    fd = open(obj_path, O_WRONLY | O_CREAT | O_TRUNC, 420);
    if (fd < 0)
        return;
    write(fd, obj_img, (unsigned long)n);
    close(fd);
}

static int enc_i(int rd, int rs1, int imm, int op)
{
    return op + (rd << 7) + (rs1 << 15) + ((imm & 4095) << 20);
}

static int enc_if(int rd, int rs1, int imm, int op, int f3)
{
    return op + (rd << 7) + (f3 << 12) + (rs1 << 15) + ((imm & 4095) << 20);
}

static int enc_u(int rd, int imm, int op)
{
    return op + (rd << 7) + ((imm & 1048575) << 12);
}

static int enc_s(int rs1, int rs2, int imm, int f3)
{
    int w;
    w = 35;
    w = w + ((imm & 31) << 7);
    w = w + (f3 << 12);
    w = w + (rs1 << 15);
    w = w + (rs2 << 20);
    w = w + ((imm & 4064) << 20);
    return w;
}

static int enc_r(int rd, int rs1, int rs2, int f3, int f7, int op)
{
    return op + (rd << 7) + (f3 << 12) + (rs1 << 15) + (rs2 << 20) + (f7 << 25);
}

static int enc_b(int rs1, int rs2, int imm, int funct3)
{
    int w;
    w = 99;
    w = w + ((imm & 2048) >> 4);
    w = w + ((imm & 30) << 7);
    w = w + (funct3 << 12);
    w = w + (rs1 << 15);
    w = w + (rs2 << 20);
    w = w + ((imm & 2016) << 20);
    w = w + ((imm & 4096) << 19);
    return w;
}

static int gas_reg(const char *s)
{
    int n;
    if (!s || !s[0])
        return -1;
    if (s[0] == 'x' && s[1] >= '0' && s[1] <= '9') {
        n = atoi(s + 1);
        if (n >= 0 && n <= 31)
            return n;
        return -1;
    }
    if (s[0] == 'a' && s[1] >= '0' && s[1] <= '7' && s[2] == 0)
        return 10 + (s[1] - '0');
    if (strcmp(s, "zero") == 0)
        return 0;
    if (strcmp(s, "ra") == 0)
        return 1;
    if (strcmp(s, "sp") == 0)
        return 2;
    if (strcmp(s, "t0") == 0)
        return 5;
    if (strcmp(s, "t1") == 0)
        return 6;
    if (strcmp(s, "t2") == 0)
        return 7;
    if (strcmp(s, "s0") == 0)
        return 8;
    if (strcmp(s, "fp") == 0)
        return 8;
    if (strcmp(s, "s1") == 0)
        return 9;
    if (s[0] == 's' && s[1] >= '2' && s[1] <= '9' && s[2] == 0)
        return 16 + (s[1] - '0');
    if (strcmp(s, "s10") == 0)
        return 26;
    if (strcmp(s, "s11") == 0)
        return 27;
    if (strcmp(s, "t3") == 0)
        return 28;
    if (strcmp(s, "t4") == 0)
        return 29;
    if (strcmp(s, "t5") == 0)
        return 30;
    if (strcmp(s, "t6") == 0)
        return 31;
    if (strcmp(s, "gp") == 0)
        return 3;
    if (strcmp(s, "tp") == 0)
        return 4;
    return -1;
}

static int gas_split(char *line)
{
    int n;
    int i;

    i = 0;
    while (line[i]) {
        if (line[i] == ',' || line[i] == '\t')
            line[i] = ' ';
        i = i + 1;
    }
    n = 0;
    i = 0;
    while (line[i] && n < 8) {
        while (line[i] == ' ')
            i = i + 1;
        if (!line[i] || line[i] == '#' || line[i] == '\n' || line[i] == '\r')
            break;
        gas_tok[n] = line + i;
        n = n + 1;
        while (line[i] && line[i] != ' ')
            i = i + 1;
        if (line[i]) {
            line[i] = 0;
            i = i + 1;
        }
    }
    i = n;
    while (i < 8) {
        gas_tok[i] = 0;
        i = i + 1;
    }
    return n;
}

static int gas_line_at(int *pi, long n)
{
    int i;
    int L;

    i = *pi;
    L = 0;
    while (i < n && gas_src[i] != '\n' && L < 127) {
        gas_line[L] = gas_src[i];
        L = L + 1;
        i = i + 1;
    }
    while (i < n && gas_src[i] != '\n')
        i = i + 1;
    if (i < n)
        i = i + 1;
    gas_line[L] = 0;
    *pi = i;
    return L;
}

static int starts_dir(const char *name)
{
    int i;
    int j;

    i = 0;
    while (gas_line[i] == ' ' || gas_line[i] == '\t')
        i = i + 1;
    j = 0;
    while (name[j]) {
        if (gas_line[i] != name[j])
            return 0;
        i = i + 1;
        j = j + 1;
    }
    if (gas_line[i] && gas_line[i] != ' ' && gas_line[i] != '\t')
        return 0;
    return 1;
}

static int ascii_nbytes(void)
{
    int i;
    int n;

    i = 0;
    while (gas_line[i] && gas_line[i] != '"')
        i = i + 1;
    if (gas_line[i] != '"')
        return -1;
    i = i + 1;
    n = 0;
    while (gas_line[i] && gas_line[i] != '"') {
        if (gas_line[i] == '\\' && gas_line[i + 1]) {
            i = i + 2;
            n = n + 1;
            continue;
        }
        i = i + 1;
        n = n + 1;
    }
    return n;
}

static void ascii_put(void)
{
    int i;
    int c;

    i = 0;
    while (gas_line[i] && gas_line[i] != '"')
        i = i + 1;
    if (gas_line[i] != '"')
        return;
    i = i + 1;
    while (gas_line[i] && gas_line[i] != '"') {
        if (gas_line[i] == '\\' && gas_line[i + 1]) {
            c = gas_line[i + 1];
            if (c == '0')
                obj_put(0);
            else if (c == 'n')
                obj_put(10);
            else
                obj_put(c);
            i = i + 2;
            continue;
        }
        obj_put((int)gas_line[i]);
        i = i + 1;
    }
}

static int handle_align(int pc, int emit)
{
    int a;

    if (!gas_tok[1])
        return pc;
    a = atoi(gas_tok[1]);
    if (a <= 0)
        return pc;
    while (pc % a) {
        if (emit)
            obj_put(0);
        pc = pc + 1;
    }
    return pc;
}

static int gas_is_lab(int ln)
{
    if (ln <= 0)
        return 0;
    if (gas_tok[0][ln - 1] == ':')
        return 1;
    return 0;
}

static int gas_is_dot(void)
{
    if (gas_tok[0][0] == '.')
        return 1;
    return 0;
}

static int gas_isz(void)
{
    if (strcmp(gas_tok[0], "jal") == 0)
        return 8;
    if (strcmp(gas_tok[0], "lla") == 0)
        return 8;
    return 4;
}

static void lab_add(const char *s, int pc)
{
    int i;
    if (nlab >= 64)
        return;
    i = 0;
    while (s[i] && s[i] != ':' && i < 31) {
        labn[nlab][i] = s[i];
        i = i + 1;
    }
    labn[nlab][i] = 0;
    labpc[nlab] = pc;
    nlab = nlab + 1;
    if (ndef < 16) {
        i = 0;
        while (s[i] && s[i] != ':' && i < 15) {
            defn[ndef][i] = s[i];
            i = i + 1;
        }
        defn[ndef][i] = 0;
        defat[ndef] = pc;
        ndef = ndef + 1;
    }
}

static void und_add(const char *s, int pc)
{
    int i;
    if (nund >= 16)
        return;
    i = 0;
    while (s[i] && i < 15) {
        undn[nund][i] = s[i];
        i = i + 1;
    }
    undn[nund][i] = 0;
    undat[nund] = pc;
    nund = nund + 1;
}

static void set_path(char *dst, const char *s)
{
    int i;
    i = 0;
    while (s[i] && i < 63) {
        dst[i] = s[i];
        i = i + 1;
    }
    dst[i] = 0;
}
static int lab_find(const char *s)
{
    int i;
    i = 0;
    while (i < nlab) {
        if (strcmp(labn[i], s) == 0)
            return labpc[i];
        i = i + 1;
    }
    return -1;
}

static int gas_emit_line(int pc)
{
    int rd;
    int rs1;
    int rs2;
    int imm;
    int tr;
    int off;
    int hi;
    int lo;
    int tgt;

    if (strcmp(gas_tok[0], "ecall") == 0) {
        obj_emit(enc_i(0, 0, 0, 115));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "jal") == 0) {
        if (!gas_tok[1] || !gas_tok[2])
            return -1;
        rd = gas_reg(gas_tok[1]);
        tgt = lab_find(gas_tok[2]);
        if (rd < 0)
            return -1;
        if (rd)
            tr = rd;
        else
            tr = 5;
        if (tgt < 0) {
            und_add(gas_tok[2], pc);
            obj_emit(enc_u(tr, 0, 23));
            obj_emit(enc_i(rd, tr, 0, 103));
            return pc + 8;
        }
        off = tgt - pc;
        hi = (off + 2048) >> 12;
        lo = off - (hi << 12);
        obj_emit(enc_u(tr, hi, 23));
        obj_emit(enc_i(rd, tr, lo, 103));
        return pc + 8;
    }
    if (strcmp(gas_tok[0], "lla") == 0) {
        if (!gas_tok[1] || !gas_tok[2])
            return -1;
        rd = gas_reg(gas_tok[1]);
        tgt = lab_find(gas_tok[2]);
        if (rd < 0 || tgt < 0)
            return -1;
        off = tgt - pc;
        hi = (off + 2048) >> 12;
        lo = off - (hi << 12);
        obj_emit(enc_u(rd, hi, 23));
        obj_emit(enc_i(rd, rd, lo, 19));
        return pc + 8;
    }
    if (strcmp(gas_tok[0], "lui") == 0) {
        if (!gas_tok[1] || !gas_tok[2])
            return -1;
        rd = gas_reg(gas_tok[1]);
        imm = atoi(gas_tok[2]);
        if (rd < 0)
            return -1;
        obj_emit(enc_u(rd, imm, 55));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "mv") == 0) {
        if (!gas_tok[1] || !gas_tok[2])
            return -1;
        rd = gas_reg(gas_tok[1]);
        rs1 = gas_reg(gas_tok[2]);
        if (rd < 0 || rs1 < 0)
            return -1;
        obj_emit(enc_i(rd, rs1, 0, 19));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "beq") == 0) {
        if (!gas_tok[1] || !gas_tok[2] || !gas_tok[3])
            return -1;
        rs1 = gas_reg(gas_tok[1]);
        rs2 = gas_reg(gas_tok[2]);
        imm = atoi(gas_tok[3]);
        if (rs1 < 0 || rs2 < 0)
            return -1;
        obj_emit(enc_b(rs1, rs2, imm, 0));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "bne") == 0) {
        if (!gas_tok[1] || !gas_tok[2] || !gas_tok[3])
            return -1;
        rs1 = gas_reg(gas_tok[1]);
        rs2 = gas_reg(gas_tok[2]);
        imm = atoi(gas_tok[3]);
        if (rs1 < 0 || rs2 < 0)
            return -1;
        obj_emit(enc_b(rs1, rs2, imm, 1));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "blt") == 0) {
        if (!gas_tok[1] || !gas_tok[2] || !gas_tok[3])
            return -1;
        rs1 = gas_reg(gas_tok[1]);
        rs2 = gas_reg(gas_tok[2]);
        imm = atoi(gas_tok[3]);
        if (rs1 < 0 || rs2 < 0)
            return -1;
        obj_emit(enc_b(rs1, rs2, imm, 4));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "bge") == 0) {
        if (!gas_tok[1] || !gas_tok[2] || !gas_tok[3])
            return -1;
        rs1 = gas_reg(gas_tok[1]);
        rs2 = gas_reg(gas_tok[2]);
        imm = atoi(gas_tok[3]);
        if (rs1 < 0 || rs2 < 0)
            return -1;
        obj_emit(enc_b(rs1, rs2, imm, 5));
        return pc + 4;
    }
    if (gas_tok[1] && gas_tok[2] && gas_tok[3]) {
        rs2 = gas_reg(gas_tok[3]);
        if (rs2 >= 0) {
            rd = gas_reg(gas_tok[1]);
            rs1 = gas_reg(gas_tok[2]);
            if (rd >= 0 && rs1 >= 0) {
                if (strcmp(gas_tok[0], "add") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 0, 0, 51));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "sub") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 0, 32, 51));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "and") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 7, 0, 51));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "or") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 6, 0, 51));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "xor") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 4, 0, 51));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "sll") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 1, 0, 51));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "srl") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 5, 0, 51));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "sra") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 5, 32, 51));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "addw") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 0, 0, 59));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "subw") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 0, 32, 59));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "sllw") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 1, 0, 59));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "srlw") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 5, 0, 59));
                    return pc + 4;
                }
                if (strcmp(gas_tok[0], "sraw") == 0) {
                    obj_emit(enc_r(rd, rs1, rs2, 5, 32, 59));
                    return pc + 4;
                }
            }
        }
    }
    if (!gas_tok[1] || !gas_tok[2] || !gas_tok[3])
        return -1;
    rd = gas_reg(gas_tok[1]);
    rs1 = gas_reg(gas_tok[2]);
    imm = atoi(gas_tok[3]);
    if (rd < 0 || rs1 < 0)
        return -1;
    if (strcmp(gas_tok[0], "addi") == 0) {
        obj_emit(enc_i(rd, rs1, imm, 19));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "addiw") == 0) {
        obj_emit(enc_i(rd, rs1, imm, 27));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "andi") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 19, 7));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "ori") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 19, 6));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "xori") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 19, 4));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "slli") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 19, 1));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "srli") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 19, 5));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "srai") == 0) {
        obj_emit(enc_if(rd, rs1, imm | 1024, 19, 5));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "slliw") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 27, 1));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "srliw") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 27, 5));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "sraiw") == 0) {
        obj_emit(enc_if(rd, rs1, imm | 1024, 27, 5));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "jalr") == 0) {
        obj_emit(enc_i(rd, rs1, imm, 103));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "ld") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 3, 3));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "lb") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 3, 0));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "lh") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 3, 1));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "lw") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 3, 2));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "lbu") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 3, 4));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "lhu") == 0) {
        obj_emit(enc_if(rd, rs1, imm, 3, 5));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "sd") == 0) {
        obj_emit(enc_s(rs1, rd, imm, 3));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "sw") == 0) {
        obj_emit(enc_s(rs1, rd, imm, 2));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "sh") == 0) {
        obj_emit(enc_s(rs1, rd, imm, 1));
        return pc + 4;
    }
    if (strcmp(gas_tok[0], "sb") == 0) {
        obj_emit(enc_s(rs1, rd, imm, 0));
        return pc + 4;
    }
    return -1;
}

static int assemble(const char *src)
{
    int fd;
    long n;
    int i;
    int nt;
    int ln;
    int pc;
    int r;

    obj_nins = 0;
    nlab = 0;
    nund = 0;
    ndef = 0;
    fd = open(src, O_RDONLY, 0);
    if (fd < 0)
        return 1;
    n = read(fd, gas_src, 2047);
    close(fd);
    if (n <= 0)
        return 1;
    gas_src[n] = 0;
    i = 0;
    pc = 0;
    while (i < n) {
        gas_line_at(&i, n);
        if (starts_dir(".ascii")) {
            r = ascii_nbytes();
            if (r < 0)
                return 1;
            pc = pc + r;
            continue;
        }
        nt = gas_split(gas_line);
        if (nt <= 0)
            continue;
        ln = 0;
        while (gas_tok[0][ln])
            ln = ln + 1;
        if (gas_is_lab(ln)) {
            lab_add(gas_tok[0], pc);
            continue;
        }
        if (strcmp(gas_tok[0], ".align") == 0) {
            pc = handle_align(pc, 0);
            continue;
        }
        if (gas_is_dot())
            continue;
        pc = pc + gas_isz();
    }
    i = 0;
    pc = 0;
    obj_nins = 0;
    while (i < n) {
        gas_line_at(&i, n);
        if (starts_dir(".ascii")) {
            r = ascii_nbytes();
            if (r < 0)
                return 1;
            ascii_put();
            pc = pc + r;
            continue;
        }
        nt = gas_split(gas_line);
        if (nt <= 0)
            continue;
        ln = 0;
        while (gas_tok[0][ln])
            ln = ln + 1;
        if (gas_is_lab(ln))
            continue;
        if (strcmp(gas_tok[0], ".align") == 0) {
            pc = handle_align(pc, 1);
            continue;
        }
        if (gas_is_dot())
            continue;
        r = gas_emit_line(pc);
        if (r < 0)
            return 1;
        pc = r;
    }
    while (obj_nins & 3)
        obj_put(0);
    if (obj_nins < 4)
        return 1;
    obj_flush();
    return 0;
}

void gas_parse(void)
{
    puts("as parse");
    set_path(obj_path, "/src/ld/crt0.o");
    if (assemble("/src/ld/crt0.s")) {
        puts("as parse fail");
        return;
    }
    set_path(obj_path, "/src/ld/main.o");
    if (assemble("/src/ld/main.s")) {
        puts("as parse fail");
        return;
    }
    set_path(obj_path, "/src/ld/puts.o");
    if (assemble("/src/ld/puts.s")) {
        puts("as parse fail");
        return;
    }
    set_path(obj_path, "/src/ld/exitfn.o");
    if (assemble("/src/ld/exitfn.s")) {
        puts("as parse fail");
        return;
    }
    puts("as obj");
}

static void poke32(unsigned char *p, int w)
{
    p[0] = (unsigned char)(w & 255);
    p[1] = (unsigned char)((w >> 8) & 255);
    p[2] = (unsigned char)((w >> 16) & 255);
    p[3] = (unsigned char)((w >> 24) & 255);
}

static int load_obj(const char *path)
{
    unsigned char buf[8192];
    unsigned char *sh;
    int fd;
    long n;
    int i;
    int j;
    int shoff;
    int shnum;
    int shent;
    int type;
    int flags;
    int off;
    int size;
    int rel;
    int nu;
    int nd;
    int base;
    int w;

    fd = open(path, O_RDONLY, 0);
    if (fd < 0)
        return 1;
    n = read(fd, buf, 8192);
    close(fd);
    if (n < 0x124)
        return 1;
    if (buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F')
        return 1;
    if (rd16(buf + 16) != 1)
        return 1;
    if (rd16(buf + 18) != 243)
        return 1;
    shoff = rd32(buf + 40);
    shent = rd16(buf + 58);
    shnum = rd16(buf + 60);
    if (shoff < 0 || shent != 64 || shnum < 2)
        return 1;
    size = 0;
    off = 0;
    i = 0;
    while (i < shnum) {
        sh = buf + shoff + i * 64;
        i = i + 1;
        type = rd32(sh + 4);
        flags = rd32(sh + 8);
        if (type != 1)
            continue;
        if ((flags & 4) == 0)
            continue;
        off = rd32(sh + 24);
        size = rd32(sh + 32);
        break;
    }
    if (off < 0 || size <= 0 || size > 4096)
        return 1;
    if (off + size > n)
        return 1;
    if (link_n + size > 4096)
        return 1;
    base = link_n;
    i = 0;
    while (i < size) {
        link_text[base + i] = buf[off + i];
        i = i + 1;
    }
    link_n = link_n + size;
    rel = off + size;
    if (rel + 12 > n)
        return 0;
    if (rd32(buf + rel) != 0x31434c52)
        return 0;
    nu = rd32(buf + rel + 4);
    nd = rd32(buf + rel + 8);
    rel = rel + 12;
    i = 0;
    while (i < nu) {
        if (rel + 20 > n)
            return 1;
        if (lnund >= 32)
            return 1;
        j = 0;
        while (j < 16) {
            lundn[lnund][j] = (char)buf[rel + j];
            j = j + 1;
        }
        lundat[lnund] = rd32(buf + rel + 16) + base;
        lnund = lnund + 1;
        rel = rel + 20;
        i = i + 1;
    }
    i = 0;
    while (i < nd) {
        if (rel + 20 > n)
            return 1;
        if (lndef >= 32)
            return 1;
        j = 0;
        while (j < 16) {
            ldefn[lndef][j] = (char)buf[rel + j];
            j = j + 1;
        }
        ldefat[lndef] = rd32(buf + rel + 16) + base;
        lndef = lndef + 1;
        rel = rel + 20;
        i = i + 1;
    }
    w = 0;
    (void)w;
    return 0;
}

static int find_def(const char *s)
{
    int i;
    i = 0;
    while (i < lndef) {
        if (strcmp(ldefn[i], s) == 0)
            return ldefat[i];
        i = i + 1;
    }
    return -1;
}

void ld_link(void)
{
    int i;
    int tgt;
    int at;
    int off;
    int hi;
    int lo;
    int w;
    int rd;
    int rs1;
    int j;

    link_n = 0;
    lnund = 0;
    lndef = 0;
    if (load_obj("/src/ld/crt0.o"))
        return;
    if (load_obj("/src/ld/main.o"))
        return;
    if (load_obj("/src/ld/puts.o"))
        return;
    if (load_obj("/src/ld/exitfn.o"))
        return;
    i = 0;
    while (i < lnund) {
        tgt = find_def(lundn[i]);
        if (tgt < 0)
            return;
        at = lundat[i];
        off = tgt - at;
        hi = (off + 2048) >> 12;
        lo = off - (hi << 12);
        w = rd32(link_text + at);
        rd = (w >> 7) & 31;
        poke32(link_text + at, enc_u(rd, hi, 23));
        w = rd32(link_text + at + 4);
        rd = (w >> 7) & 31;
        rs1 = (w >> 15) & 31;
        poke32(link_text + at + 4, enc_i(rd, rs1, lo, 103));
        i = i + 1;
    }
    elf_reset();
    j = 0;
    while (j < link_n) {
        elf_emit(rd32(link_text + j));
        j = j + 4;
    }
    elf_flush();
}

