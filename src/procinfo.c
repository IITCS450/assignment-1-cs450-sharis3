#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include "common.h"


int read_cmdline(long pid, char **out);
static int parse_pid(const char *s, long *pid_out);

int read_vmrss_kb(long pid, long *vmrss_kb);
static void usage(const char *a)
{
    fprintf(stderr, "Usage: %s <pid>\n", a);
    exit(1);
}
// static int isnum(const char *s)
// {
//     for (; *s; s++)
//         if (!isdigit(*s))
//             return 0;
//     return 1;
// }
int read_stat_fields(long pid, char *state, long *ppid,
                     unsigned long long *utime, unsigned long long *stime);

int main(int c, char **v)
{
    if (c != 2)
        usage(v[0]);
    

    long pid = 0;
    if (!parse_pid(v[1], &pid))
        usage(v[0]);

    
    char st;
    long ppid;
    unsigned long long ut, stt;

    if (!read_stat_fields(pid, &st, &ppid, &ut, &stt))
    {
        perror("read_stat_fields");
        return 1;
    }

    printf("DBG parsed: state=%c ppid=%ld utime=%llu stime=%llu\n", st, ppid, ut, stt);

    char *cmd = NULL;
    if (!read_cmdline(pid, &cmd)) {perror("read_cmdline"); return 1;}
    printf("DBG cmdline: %s\n", cmd);
    free(cmd);

    long vmrss= 0;
    if(!read_vmrss_kb(pid, &vmrss)) {perror("read_vmrss_kb");return 1;}
    printf("DBG vmrss: %ld\n", vmrss );

    return 0;
}

static int parse_pid(const char *s, long *pid_out)
{
    if (s == NULL || s[0] == '\0')
        return 0;
    for (const char *p = s; *p != '\0'; p++)
    {
        if (!isdigit((unsigned char)*p))
            return 0;
    }
    errno = 0;

    char *end = NULL;
    long pid = strtol(s, &end, 10);
    if (errno != 0)
        return 0;

    if (end == s || *end != '\0')
        return 0;

    if (pid <= 0)
        return 0;

    *pid_out = pid;
    return 1;
}

static int is_state_char(char c)
{
    return (c == 'R' || c == 'S' || c == 'D' || c == 'Z' || c == 'T' || c == 't' || c == 'X' || c == 'x' || c == 'K' || c == 'W' || c == 'I');
}

static char *find_rpar(char *line)
{
    size_t n = strlen(line);

    if (n == 0)
        return NULL;

    // scan from end for a ')' that is followed by: "<space(s)> <state> <space> <digit>"

    for (char *p = line + (n - 1); p > line; --p)
    {
        if (*p != ')')
            continue;

        char *after = p + 1;
        while (*after == ' ')
            after++;

        if (!is_state_char(*after))
            continue;
        after++;
        if (*after != ' ')
            continue;
        after++;

        if (!isdigit((unsigned char)*after))
            continue;
        return p;
    }

    return NULL;
}

int read_stat_fields(long pid, char *state, long *ppid,
                     unsigned long long *utime, unsigned long long *stime)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/stat", pid);

    FILE *f = fopen(path, "r");
    if (!f)
        return 0;

    char line[8192];
    if (!fgets(line, sizeof(line), f))
    {
        fclose(f);
        return 0;
    }

    fclose(f);

    char *lpar = strchr(line, '(');
    char *rpar = find_rpar(line);

    if (!lpar || !rpar || rpar < lpar)
    {
        errno = EINVAL;
        return 0;
    }

    char *after = rpar + 1;
    while (*after && isspace((unsigned char)*after))
        after++;

    const int UTIME_TOK = 11;
    const int STIME_TOK = 12;

    int tok = 0;
    char st = '?';
    long parent = -1;
    unsigned long long u = 0, s = 0;

    char *save = NULL;

    for (char *t = strtok_r(after, " ", &save);
         t;
         t = strtok_r(NULL, " ", &save), tok++)
    {

        if (tok == 0)
            st = t[0];
        else if (tok == 1)
            parent = strtol(t, NULL, 10);
        else if (tok == UTIME_TOK)
            u = strtoull(t, NULL, 10);
        else if (tok == STIME_TOK)
        {
            s = strtoull(t, NULL, 10);
            break;
        }
    }

    if (parent < 0)
    {
        errno = EINVAL;
        return 0;
    }

    *state = st;
    *ppid = parent;
    *utime = u;
    *stime = s;

    return 1;
}

int read_cmdline(long pid, char **out) {

    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);

    FILE *f = fopen(path, "rb");

    if (!f) return 0;
    size_t cap = 256, len = 0;
    char *buf = malloc(cap);
    if (!buf) {fclose(f); errno = ENOMEM; return 0;};


    for (;;) {
        if (len == cap) {
            cap  *= 2;
            char *nb = realloc(buf, cap);
           
            if (!nb) {fclose(f); errno = ENOMEM; return 0;};
            buf = nb;
        }
        size_t n = fread(buf + len, 1, cap - len, f);
        len += n;

        if (n == 0) {
            if (ferror(f)) { free(buf); fclose(f); errno = EIO; return 0;}
            break;
        } 
    }
    fclose(f);

    if (len == 0) {
        free(buf);
        *out = strdup("[empty]");
        if (!*out) {errno = ENOMEM; return 0;}
        return 1;
    }

    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\0') buf[i] = ' ';
    }

    char *nb = realloc(buf, len + 1);
    if (!nb) {free(buf); errno = ENOMEM; return 0;}
    buf = nb;

    buf[len] = '\0';

    while (len > 0 && buf[len - 1] == ' ') {
        buf[len - 1] = '\0';
        len --;
    }

    if (len == 0) {
        free(buf);
        *out = strdup("[empty]");
        if (!*out) {errno = ENOMEM; return 0;}
        return 1;
    } 

    *out = buf;
    return 1;
}

int read_vmrss_kb(long pid, long *vmrss_kb)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%ld/status", pid);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[512];
    long val = 0;
    int found = 0;

    while(fgets(line, sizeof(line), f )) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            char *p = line + 6;
            while (*p && isspace((unsigned char)*p)) p++;

            errno = 0;
            val = strtol(p, NULL, 10);
            if (errno == 0) found = 1;
            break;
        }
    }

    fclose(f);

    if(!found) val = 0;
    *vmrss_kb = val;
    return 1;

}