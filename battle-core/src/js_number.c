/* Money messages use the browser's numeric locale, independently of i18next.
 * This port currently supplies the en-US numeric profile. Expand Ryu's shortest
 * decimal, rather than printing the exact binary integer: Intl formats
 * 1000000000000000128 as 1,000,000,000,000,000,100. */
#include "pr/text.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RYU_ONLY_64_BIT_OPS
#include "../vendor/ryu/d2s.c"

size_t pr_money_en_us(double value, char *out, size_t cap)
{
    char text[416], decimal[32], digits[20];
    size_t n = 0;
    if (signbit(value) && !isnan(value)) text[n++] = '-';
    if (isnan(value)) { memcpy(text + n, "NaN", 3); n += 3; }
    else if (isinf(value)) { memcpy(text + n, "\xe2\x88\x9e", 3); n += 3; }
    else if (value != trunc(value)) n = 0; /* outside the integer-money contract */
    else {
        d2s_buffered(fabs(value), decimal);
        const char *exponent = strchr(decimal, 'E');
        int nd = 0;
        for (const char *p = decimal; p < exponent; p++)
            if (*p != '.') digits[nd++] = *p;
        const int width = atoi(exponent + 1) + 1;
        for (int i = 0; i < width; i++) {
            if (i && (width - i) % 3 == 0) text[n++] = ',';
            text[n++] = i < nd ? digits[i] : '0';
        }
    }
    if (cap && out) {
        size_t copy = n < cap - 1 ? n : cap - 1;
        memcpy(out, text, copy);
        out[copy] = 0;
    }
    return n;
}
