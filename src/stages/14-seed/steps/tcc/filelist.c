/* filelist OUT DIR: every regular file under DIR (walked recursively,
 * symlinks followed), as DIR/..., sorted bytewise, one per line, into OUT:
 * the LIST of stage 13's `mkustar TAR ROOT LIST`, run from DIR's parent with
 * ROOT ".". Anything but a directory or a regular file is an error (mkustar
 * packs only files; it writes their parent directories itself).
 * Prints "filelist ok N files". */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char **paths;
static unsigned long npaths;
static unsigned long cap;

static void die(const char *a, const char *b)
{
    printf("filelist: %s%s\n", a, b);
    exit(1);
}

static void walk(const char *dir)
{
    DIR *d;
    struct dirent *e;
    struct stat st;
    char *p;

    d = opendir(dir);
    if (!d)
        die("cannot open directory ", dir);
    while ((e = readdir(d)) != 0) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        p = malloc(strlen(dir) + strlen(e->d_name) + 2);
        if (!p)
            die("out of memory", "");
        sprintf(p, "%s/%s", dir, e->d_name);
        if (stat(p, &st) != 0)
            die("cannot stat ", p);
        if (S_ISDIR(st.st_mode)) {
            walk(p);
            free(p);
        } else if (S_ISREG(st.st_mode)) {
            if (npaths == cap) {
                cap = cap ? cap * 2 : 256;
                paths = realloc(paths, cap * sizeof(char *));
                if (!paths)
                    die("out of memory", "");
            }
            paths[npaths] = p;
            npaths = npaths + 1;
        } else {
            die("not a directory or regular file: ", p);
        }
    }
    closedir(d);
}

static int cmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

int main(int argc, char **argv)
{
    FILE *f;
    unsigned long i;

    if (argc != 3)
        die("usage: filelist OUT DIR", "");
    walk(argv[2]);
    qsort(paths, npaths, sizeof(char *), cmp);
    f = fopen(argv[1], "w");
    if (!f)
        die("cannot create ", argv[1]);
    for (i = 0; i < npaths; i = i + 1)
        fprintf(f, "%s\n", paths[i]);
    if (fclose(f) != 0)
        die("cannot write ", argv[1]);
    printf("filelist ok %lu files\n", npaths);
    return 0;
}
