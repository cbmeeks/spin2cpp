#ifndef UTIL_H_2016_
#define UTIL_H_2016_

#include <stdarg.h>
#include <wchar.h>

#if 0
#define va_ptr          va_list
#define va_ptrarg(x, t) va_arg(x, t)
#else
#define va_ptr          va_list *
#define va_ptrarg(x, t) va_arg(*x, t)
#endif


/* internal structures and defines for printf */
typedef struct printf_info {
    // per argument state
    int width;
    int prec;
    int pad;                 // padding character '0' or ' '
    int spec;                // actual specification character
    unsigned int alt   :1;   // the '#' flag
    unsigned int space :1;
    unsigned int left  :1;   // the '-' flag was specified
    unsigned int showsign :1; // the '+' flag
    unsigned int longflag :1; // 'l' modifier appeared (used for %ls, %lc)

    int size;    // size of argument in bytes

    // global state
    int byteswritten;
    int (*putchar)(int c, void *arg);
    void *putarg;
} Printf_info;

typedef int (*FmtPutchar)(int, void *);

int dofmt( FmtPutchar func, void *funcarg, const char *fmt, va_ptr args );

// convert an integer to a string using a specified base and precision
int lltoa_prec( unsigned long long x, char *buf, unsigned base, int prec );

// make a string upper case (in place)
char* strupr(char *origstr);

// reverse a string (in place)
char *strrev(char *origstr);

// create a new string with the concatenation of two old ones
char *strdupcat(const char *a, const char *b);

// convert wide character wcorig to utf-8 in s, returns count
size_t to_utf8(char *s, wchar_t wcorig);

// convert utf-8 sequence to wide character; n is max size of cptr
size_t from_utf8(wchar_t *wcptr, const char *cptr, size_t n);

#endif
