
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int vasprintf(char **result, const char *format, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    const int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) return -1;
    *result = (char *)malloc((size_t)length + 1u);
    if (*result == NULL) return -1;
    return vsnprintf(*result, (size_t)length + 1u, format, args);
}
