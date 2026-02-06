#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include "common.h"

static int parse_pid(const char *s, long *pid_out);
static void usage(const char *a)
{
    fprintf(stderr, "Usage: %s <pid>\n", a);
    exit(1);
}
static int isnum(const char *s)
{
    for (; *s; s++)
        if (!isdigit(*s))
            return 0;
    return 1;
}
int read_stat_fields(long pid, char *state, long *ppid,
                     unsigned long long *utime, unsigned long long *stime);

int main(int c, char **v)
{
    if (c != 2)
        usage(v[0]);
    ;

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