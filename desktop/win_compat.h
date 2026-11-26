/* Windows-only POSIX compatibility for the shared C source. */
#ifndef UAPLAY_WIN_COMPAT_H
#define UAPLAY_WIN_COMPAT_H
#include <direct.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <io.h>
#define mkdir(path,mode) _mkdir(path)
/* Portable reentrant tokenization, including consecutive delimiters. */
static inline char *preview_strtok_r(char *str,const char *delim,char **next){
    char *p=str?str:*next;
    if(!p)return NULL;
    p+=strspn(p,delim);
    if(!*p){*next=p;return NULL;}
    char *end=p+strcspn(p,delim);
    if(*end)*end++=0;
    *next=end;return p;
}
#define strtok_r preview_strtok_r
#endif
