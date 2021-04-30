/* ******************************************************************* *
 
    FSJ - File Splitter & Joiner

 * ********************************************************* *
 * By Yuriy Y. Yermilov aka (binaryONE) cyclone.yyy@gmail.com
 *
 * binary001.blogspot.com
 * www.myspace.com/binaryone
 * www.devrc.org (private)
 *
 * THIS SOFTWARE IS PROVIDED BY THE OWNER ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE OWNER
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ********************************************************** */

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define IO_MAX        0x10000   // test diff values for best
                                // performance (64KB seems to work best)

void show_banner(void);
void show_usage(void);
int check_args(int n, char **args);
void split_file(char *file);
void join_file(char *file);
void set_binwrt_mode(int fd);
void p_error(int err);
void init_errors(void);
void read_write(char *buf, int fd);
void attach_header(int fd);
void strip_header(int fd);
void itos(int n, char s[], int *size);
int gotoc(char *str, char c, int metac, int stopc);
char *revs(char *str);
int nchar(char *s, char c);
int hasalpha(char *str, int size);

#ifdef _DOSLIKE
#define CREATE_FLAGS S_IREAD|S_IWRITE
#define OPEN_FLAGS O_BINARY|O_RDONLY
#else
#define OPEN_FLAGS 0
#define CREATE_FLAGS S_IRUSR|S_IWUSR
#endif

struct _globals {
    int        parts;
    unsigned long    part_size;
    int        io_size;
    int        fd;
    struct stat    fst;
} g;

#define N_ERRORS 10

struct _errors {
    int    id;
    char    *s;
};

struct _errors *fs_errors;

int
main(int ac, char **av)
{
    init_errors();
    show_banner();

    if(check_args(ac, av))
        split_file(av[2]);
    else
        join_file(av[2]);
    return 0;
}

void
show_banner(void)
{
printf("\n"
    "FSJ - File Splitter & Joiner v.009_6226\n"
    "by Yuriy Y. Yermilov.\n"
    "Open-Source, Please read the disclaimer in fsj.c\n"
    "Contact info included.\n");
}

void
show_usage(void)
{
printf("\n"
    "-h [print this message]\n"
    "-n<parts> [define # of parts to split file into]\n"
    "-s<part_size> [splits file into file_size/<your value (in bytes)>]\n"
    "-j <name_of_join_file.fsj> [joins files back together]\n"
    "To split file into 10 parts (at most):\n"
    "Example [#1]: fsj -n10 FILE.EXT\n"
    "To splits file into N parts 512 bytes each (at most):\n"
    "Example [#2]: fsj -s512 FILE.EXT\n"
    "To join files specified in FILE_name.fsj into file named <name>.\n"
    "Example [#3]: fsj -j FILE_name.fsj\n");

    exit(911);
}

void
init_errors(void)
{
    static struct _errors errors[N_ERRORS] = {
        {0, "Usage: fsj <option> <file>\n"},
        {1, "Cannot open file for reading.\n"},
        {2, "Cannot get stat of file.\n"},
        {3, "No need to split.\n"},
        {4, "Cannot creat file. Aborting...\n"},
        {5, "Cannot read from file. Aborting...\n"},
        {6, "Cannot write to file. Aborting...\n"},
        {7, "Part and/or Part_size parameters must be > 0.\n"},
        {8, "Cannot write to join info file. Aborting...\n"},
        {9, "Cannot get stats of join file. Aborting...\n"},
    };

    fs_errors = &errors[0];
}

int
check_args(int np, char **arg)
{
    if(np != 3)
        show_usage();

    if(arg[1][0] == '-' && (arg[1][1] == 'n' || arg[1][1] == 's' || \
       arg[1][1] == 'j')) {
        if((g.fd = open(arg[2], OPEN_FLAGS)) < 0)
            p_error(1);
        else if(hasalpha(&arg[1][2], strlen(&arg[1][2])))
            show_usage();
        else if(arg[1][1] == 'n') {
            g.parts = atoi(&arg[1][2]);
            g.part_size = 0;
        } else if(arg[1][1] == 's')
            g.part_size = atoi(&arg[1][2]);
        else if(arg[1][1] == 'j')
            return 0;
        else
            show_usage();
        return 1;
    } else
        show_usage();

    return 0;    // shouldn't get here
}

void
join_file(char *fname)
{
    char    *fnbuf;
    char    *newf;
    char    rwbuf[IO_MAX];
    char    *s;
    int    fnbytes;
    int    fd, nfd;
    int    i, j;

    /* ********************************************************** *
     * 1. Read (if possible) the join file specified into buffer.
     * 2. Open each file. (names separated by \030)
     * 3. Create (overwrite and warn user) the join_<NAME> file.
     * 4. Append bytes to <NAME> file.
     * ********************************************************** */

    if((fd = open(fname, OPEN_FLAGS)) < 0)
        p_error(1);
    else if(fstat(fd, &g.fst))
        p_error(9);

    fnbytes = g.fst.st_size;
    fnbuf = (char *)malloc(fnbytes);
    s = (char *)malloc(fnbytes);

    i = gotoc(fname, '_', 0, '\000');        // 1st underscore
    j = gotoc(&fname[i + 1], '.', -1, '\000');    // last period

    newf = (char *)malloc(j);
    memmove(newf, &fname[i + 1], j);

    if((nfd = creat(newf, CREATE_FLAGS)) < 0)
        p_error(4);

    set_binwrt_mode(nfd);

    if((read(fd, fnbuf, fnbytes)) != fnbytes)
        p_error(5);

    for(j = i = 0; i < fnbytes; i++, j++) {
        if(fnbuf[i] == '\030') {
            s[j] = '\000';
            if((g.fd = open(s, OPEN_FLAGS)) < 0)
                p_error(1);
            else if(fstat(g.fd, &g.fst))
                p_error(9);
            g.part_size = g.fst.st_size;
            printf("Processing file [%s], size [%lu] bytes.\n",\
            s, g.part_size);
            read_write(rwbuf, nfd);
            j = -1;
        } else
            s[j] = fnbuf[i];
    }

    free(newf);
    free(fnbuf);
    free(s);
}

void
split_file(char *file)
{
    char *newf,
         *buf,
         s[128],
         **f_names,
         *joinf;
    int  i, nfd, jfd, j,
         fs,        /* file name size */
         nfs;        /* new name file size */
    unsigned long    tail = 0;

    /* ********************************************************** *
        1. get file size.
        2. check to see if part_size > file size.
        3. find out how many parts file can be divided into.
        4. create that many temporary files.
        5. copy part_size number of bytes into each of them.
     * ********************************************************** */

    if(fstat(g.fd, &g.fst) || g.parts <= 1)
        p_error(3);

    printf("File size is: [%lu] bytes.\n", (unsigned long)g.fst.st_size);

    if(!g.part_size && !g.parts)
        p_error(7);

    if(g.part_size) {
        if(g.fst.st_size <= (signed)g.part_size)
            p_error(4);
    } else
         g.part_size = (g.fst.st_size / g.parts);


    tail = g.fst.st_size % g.parts;

    printf("Maximum even parts allowed to split file into, is [%ld].\n", \
           (g.fst.st_size / g.part_size));
    printf("Number of Left Over Bytes is [%lu].\n", tail);
    printf("Splitting: file [%s] into [%d] equal parts, each [%lu] \
 bytes in size.\n", \
        file, g.parts, g.part_size);
    printf("Writing remaining (uneven) [%lu] bytes to file #[%d].\n", \
        tail, g.parts + (tail ? 1 : 0));


    /* ************************************************************* *
        1. Create "parts" number of files.
        2. Name each one exactly as "file" and add the part
           number at the end.
        3. update f_names as we're splitting.
     * ************************************************************* */


    f_names = (char **)malloc(g.parts + 2);

    fs = strlen(file);
    newf = (char *)malloc(fs + 10);

    joinf = (char *)malloc(fs + 10);
    memmove(joinf, "join_", 5);
    memmove(&joinf[5], file, fs);
    memmove(&joinf[fs + 5], ".fsj", 4);
    if((jfd = creat(joinf, CREATE_FLAGS)) < 0)
        p_error(4);

    buf = (char *)malloc(IO_MAX + 1);
    memset(newf, 0, fs + 10);
    memmove(newf, file, fs);

    for(i = 0; i < g.parts; i++) {
        itos(i, s, &nfs);
    _l_over:
        j = strlen(s);
        memmove(&newf[fs], s, j);
        f_names[i] = (char *)malloc(fs + j + 10 + 1);
        memset(f_names[i], 0, fs + j + 10 + 1);
        memmove(f_names[i], newf, fs + j);
        printf("new_file_name: [%s]\n", f_names[i]);
        if((nfd = creat(f_names[i], CREATE_FLAGS)) < 0)
            p_error(4);
        set_binwrt_mode(nfd);
        read_write(buf, nfd);
        if((write(jfd, f_names[i], fs + j)) != fs + j)
            p_error(8);
        else if((write(jfd, "\030", 1)) != 1)
            p_error(8);
    }

    if(tail) {
        itos(g.parts, s, &nfs);
        g.part_size = tail;
        i++;
        tail = 0;
        goto _l_over;
    }

    /* ************************************************************* *
     *     Free memory.
     * ************************************************************* */

    for(i = 0; i < g.parts; i++)
        free(f_names[i]);

    free(f_names);
    free(newf);
    free(joinf);
    free(buf);
}

void
set_binwrt_mode(int fd)
{
#ifdef _DOSLIKE
    /* ************************************************************* *
        DOS/WIN files need to have
        O_BINARY (0x0004) set or else 0x0D (\r)
        will be appended before each 0x0A(\n).
     * ************************************************************* */
        setmode(fd, 0x0004);
#endif
}

void
read_write(char *buf, int fd)
{
    int            tr;        // to read
    unsigned long        tail = 0,    // left over bytes
                     p_size;        // part size
    static int        parts;
    int i;
    
    p_size = g.part_size;

    (p_size < IO_MAX) ? (tr = p_size) : (tr = IO_MAX);

    parts = (p_size / tr);

    if(p_size % tr)
        tail = p_size - (tr * parts);
_loop:
    while(parts--) {
        if((i = read(g.fd, buf, tr)) != tr)
            p_error(5);
        else if((write(fd, buf, tr)) != tr)
            p_error(6);
    }

    if(tail) {
        tr = tail;
        parts = 1;
        tail = 0;
        goto _loop;
    }
}

void
p_error(int err)
{
    fprintf(stderr, "error code: [%d]\n", err);
    fprintf(stderr, "%s", fs_errors[err].s);
    exit(911);
}

void
itos(int n, char s[], int *size)
{
    register int i = 0;
    int max_neg = 0, neg = 0;

    if(n < 0)
        neg = 1;
    if(n == -2147483647)
        max_neg  = 1;
    if(neg) {
        if(max_neg)
            n += 10;
        n = -n;
    }

    do {
        if(max_neg && i == 1)
            ++n;
        s[i++] = n % 10 + '0';
        n /= 10;
    } while (n > 0);

    if(neg)
        s[i++] = '-';
    s[i++] = '\0';

    if(size != NULL)
        *(size) = i;

    revs(s);
}

char *
revs(char *str)
{
    register char *s = str;
    register char *end;
    char c;

    if(*s) {
        end = s + strlen(s) - 1;
        while (s < end) {
            c = *s;
            *s = *end;
            *end = c;
            s++, end--;
        }
    }
    return (s);
}

int
gotoc(char *str, char c, int metac, int stopc)
{
    register char *s = str;
    register int i;
    int n = 0;
    int t = nchar(s, c);        // total # of c in str

    if(metac <= -1)            // keep user input in bounds
        metac = t;

    for(i = 0; *s != stopc; s++, i++) {
        if(*s == c) {
            n++;
            if(metac == 0)        // FIRST
                return i;
            else if(n == t)     // LAST
                return i;
            else if(n == metac)    // USER SELECTED
                return i;
        }
    }

    if(!n)
        //return -1;
        return 0;
    return i;
}

int
nchar(char *s, char c)
{
    register int n = 0;

    while(*s)
        if(*s++ == c)
            n++;
    return n;
}

int
hasalpha(char *str, int size)
{
    register char *s = str;
    register int i;

    for(i = 0; isascii(s[i]) && i < size; i++)
        if(isalpha(s[i]))
            return 1;
    return 0;
}
