#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>


int parse_pid(const char *s, long *pid_out) {
    if (s == NULL || s[0] == '\0') return 0;
    for (const char *p = s; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p))
            return 0;
    }
    errno = 0;

    char *end = NULL;
    long pid = strtol(s, &end, 10);
    if (errno !=0)
        return 0;
    
    if (end == s || *end != '\0')
        return 0;

    if (pid <=0)
        return 0;
    
    *pid_out = pid;
    return 1;
}
int main() {
    

    long pid;
    printf("%d\n", parse_pid("1234", &pid));   
    printf("%ld\n", pid);                     
    printf("%d\n", parse_pid("abc", &pid));   
    printf("%d\n", parse_pid("-5", &pid));     
    printf("%d\n", parse_pid("12x", &pid));    
    printf("%d\n", parse_pid("", &pid));   
}
