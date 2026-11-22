#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *data;
static Elf64_Shdr *sections;
static const char *names;
static Elf64_Ehdr *eh;
static Elf64_Shdr *section(const char *name)
{
    unsigned i;
    for (i=0; i<eh->e_shnum; i++)
        if (!strcmp(names+sections[i].sh_name,name)) return &sections[i];
    return NULL;
}
static Elf64_Sym *symbol(const char *name)
{
    Elf64_Shdr *tab=section(".symtab");
    Elf64_Sym *symbols=(void *)(data+tab->sh_offset);
    const char *strings=(void *)(data+sections[tab->sh_link].sh_offset);
    unsigned i;
    for (i=0; i<tab->sh_size/sizeof(*symbols); i++)
        if (!strcmp(strings+symbols[i].st_name,name)) return &symbols[i];
    return NULL;
}
int main(int argc, char **argv)
{
    FILE *f;
    long size;
    Elf64_Shdr *ex, *text;
    Elf64_Sym *low,*high,*fix,*needed;
    uint32_t flag;
    unsigned i;
    if (argc!=2 || !(f=fopen(argv[1],"rb"))) return 1;
    fseek(f,0,SEEK_END); size=ftell(f); rewind(f);
    if (size<64 || !(data=malloc(size)) || fread(data,1,size,f)!=(size_t)size) return 2;
    fclose(f); eh=(void *)data;
    if (memcmp(eh->e_ident,ELFMAG,SELFMAG) || eh->e_ident[EI_CLASS]!=ELFCLASS64 ||
        eh->e_shoff+eh->e_shnum*sizeof(Elf64_Shdr)>(size_t)size) return 3;
    sections=(void *)(data+eh->e_shoff);
    names=(void *)(data+sections[eh->e_shstrndx].sh_offset);
    ex=section("__ex_table"); text=section(".text");
    low=symbol("probe_low"); high=symbol("probe_high"); fix=symbol("probe_fix");
    needed=symbol("main_extable_sort_needed");
    if (!ex || !text || !low || !high || !fix || !needed || ex->sh_size!=24 ||
        ex->sh_offset<text->sh_offset+0xf000) return 4;
    memcpy(&flag,data+sections[needed->st_shndx].sh_offset+
           needed->st_value-sections[needed->st_shndx].sh_addr,4);
    if (flag) return 5;
    for(i=0;i<2;i++) {
        int32_t insn, fixup;
        uint16_t type;
        unsigned char *entry=data+ex->sh_offset+i*12;
        uint64_t base=ex->sh_addr+i*12;
        memcpy(&insn,entry,4); memcpy(&fixup,entry+4,4); memcpy(&type,entry+8,2);
        if ((uint64_t)((int64_t)base+insn)!=(i ? high : low)->st_value ||
            (uint64_t)((int64_t)base+4+fixup)!=fix->st_value || type!=(i ? 1 : 2)) return 6;
    }
    puts("linux sorttable entries and flag ok");
    free(data);
    return 0;
}
