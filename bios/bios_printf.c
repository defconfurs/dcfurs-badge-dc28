
#include <stdarg.h>
#include <stdint.h>

extern void _putchar(int ch);

#define PRINTF_FLAG_JUSTIFY     (1 << 0)
#define PRINTF_FLAG_ZERO_PAD    (1 << 1)
#define PRINTF_FLAG_SIGN_PAD    (1 << 2)
#define PRINTF_FLAG_FORCE_SIGN  (1 << 3)

#define PRINTF_DIGITS_SIZE      (sizeof(uint32_t) * 3 + 1)

/* Enable/disable ultra-smallness by restricting the escape sequence parser. */
#define PRINTF_FULL_PARSE 0

/* The version built into libc uses a lookup table - we don't have space for that. */
static inline int bios_isdigit(int x)
{
    return (x >= '0') && (x <= '9');
}

static inline const char *bios_strchr(const char *str, int ch)
{
    do {
        if (*str == ch) return str;
    } while (*str++ != '\0');
    return 0;
}

static inline int bios_strlen(const char *str)
{
    const char *start = str;
    while (*str != '\0') str++;
    return (str - start);
}

/* Bithacking to get a divide-by-10 */
/* Copied from the 'Hacker's Delight' algorithm */
static inline unsigned int
bios_div10(unsigned int n)
{
    unsigned int q, r;
    q = (n >> 1) + (n >> 2);
    q = q + (q >> 4);
    q = q + (q >> 8);
    q = q + (q >> 16);
    q = q >> 3;
    r = n - (q << 1) - (q << 3);
    return q + (r > 9);
}

static char *
bios_hex_digits(uint32_t value, char alpha, char *outbuf)
{
    int offset = PRINTF_DIGITS_SIZE;
    outbuf[--offset] = '\0';

    /* Special case */
    if (value == 0) {
        outbuf[--offset] = '0';
    }
    /* Build the hex string */
    while (value) {
        int nibble = value & 0xF;
        value >>= 4;
        outbuf[--offset] = (nibble >= 10) ? (alpha + nibble - 10) : ('0' + nibble);
    }

    return &outbuf[offset];
}

static char *
bios_octal_digits(uint32_t value, char *outbuf)
{
    int offset = PRINTF_DIGITS_SIZE;
    outbuf[--offset] = '\0';

    /* Special case */
    if (value == 0) {
        outbuf[--offset] = '0';
    }
    /* Build the octal string */
    while (value) {
        int octal = value & 0x7;
        value >>= 3;
        outbuf[--offset] = ('0' + octal);
    }

    return &outbuf[offset];
}

static char *
bios_decimal_digits(uint32_t value, char *outbuf)
{
    int offset = PRINTF_DIGITS_SIZE;
    outbuf[--offset] = '\0';

    /* Special Case */
    if (value == 0) {
        outbuf[--offset] = '0';
    }
    /* Build the decimal string */
    while (value) {
        unsigned int vdiv10 = bios_div10(value);
        unsigned int rem = value - (vdiv10 << 1) - (vdiv10 << 3);
        outbuf[--offset] = '0' + rem;
        value = vdiv10;
    }
    return &outbuf[offset];
}

/* Print a string of digits out the uart, while honoring the flags. */
static int
bios_print_digits(int sign, unsigned int flags, unsigned int width, const char *digits)
{
    int len = bios_strlen(digits);
    char signchar = '\0';

    /* Generate the sign character */
    if (sign < 0) signchar = '-';
    else if (flags & PRINTF_FLAG_FORCE_SIGN) signchar = '+';
    else if (flags & PRINTF_FLAG_SIGN_PAD) signchar = ' ';

    /* Count the sign as part of the digits */
    if (signchar) len++;

    /* Zero-padding version - always right-justified. */
    if (flags & PRINTF_FLAG_ZERO_PAD) {
        if (signchar) _putchar(signchar);            /* Sign */
        for (;len < width; len++) _putchar('0');     /* Padding */
        while (*digits != '\0') _putchar(*digits++); /* Digits */
    }
    /* Left-justified version. */
    else if (flags & PRINTF_FLAG_JUSTIFY) {
        if (signchar) _putchar(signchar);            /* Sign */
        while (*digits != '\0') _putchar(*digits++); /* Digits */
        for (;len < width; len++) _putchar(' ');     /* Padding */
    }
    /* Right-justified version. */
    else {
        for (;len < width; len++) _putchar(' ');     /* Padding */
        if (signchar) _putchar(signchar);            /* Sign */
        while (*digits != '\0') _putchar(*digits++); /* Digits */
    }

    return len;
}

/* TODO: Some Future code might go here to handle precision correctly. */
static inline int
bios_print_string(const char *str, unsigned int flags, int width)
{
    return bios_print_digits(0, flags & PRINTF_FLAG_JUSTIFY, width, str);
}

/* An extremely minimal printf. */
void
bios_vprintf(const char *fmt, va_list ap)
{
    char temp[PRINTF_DIGITS_SIZE];
    unsigned int flags;
    unsigned int width;
    unsigned int precision;
    char mods;
    char specifier;
    int length = 0;

    while (1) {
        char ch = *fmt++;
        unsigned int flags = 0;
        unsigned int precision = 0;
        unsigned int mods = 0;
        int width = 0;

        /* Handle non-escape sequences */
        if (ch == '\0') {
            return;
        }
        if (ch != '%') {
            _putchar(ch);
            length++;
            continue;
        }

        /* Parse the flags, if any. */
        for (;;) {
            ch = *fmt;
            if (ch == '-')      flags |= PRINTF_FLAG_JUSTIFY;
            else if (ch == '+') flags |= PRINTF_FLAG_FORCE_SIGN;
            else if (ch == ' ') flags |= PRINTF_FLAG_SIGN_PAD;
            else if (ch == '0') flags |= PRINTF_FLAG_ZERO_PAD;
            else break;
            fmt++;
        }

        /* Parse the width, if present. */
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
        }
        else {
            while (bios_isdigit(*fmt)) {
                width = (width * 10) + (*fmt++ - '0');
            }
        }

#if PRINTF_FULL_PARSE
        /* Parse out the precision, even though we don't use it. */
        if (*fmt == '.') {
            fmt++;
            if (*fmt == '*') {
                precision = va_arg(ap, int);
                fmt++;
            }
            else {
                while (bios_isdigit(*fmt)) {
                    precision = (precision * 10) + (*fmt++ - '0');
                }
            }
        }
        /* Parse the modifiers, even though we don't use them. */
        for (;;) {
            ch = *fmt;
            if (ch == 'h' || ch == 'l' || ch == 'j' || ch == 'z' || ch == 't') {
                mods = ch;
                fmt++;
            }
            else break;
        }
#else
        /* Crudely skip passed any precision or modifiers. */
        while (bios_isdigit(*fmt) || bios_strchr(".*hljzt", *fmt)) fmt++;
#endif

        /* And finally, handle the specifier */
        ch = *fmt++;
        switch (ch) {
            case '%':
                _putchar('%');
                length++;
                break;
            case 'p':
            case 'x': {
                uint32_t value = va_arg(ap, uint32_t);
                char *digits = bios_hex_digits(value, 'a', temp);
                length += bios_print_digits(0, flags, width, digits);
                break;
            }
            case 'X': {
                uint32_t value = va_arg(ap, uint32_t);
                char *digits = bios_hex_digits(value, 'A', temp);
                length += bios_print_digits(0, flags, width, digits);
                break;
            }
            case 'u': {
                uint32_t value = va_arg(ap, uint32_t);
                char *digits = bios_decimal_digits(value, temp);
                length += bios_print_digits(0, flags, width, digits);
                break;
            }
            case 'i':
            case 'd': {
                int32_t value = va_arg(ap, int32_t);
                char *digits = bios_decimal_digits((value < 0) ? -value : value, temp);
                length += bios_print_digits(value, flags, width, digits);
                break;
            }
            case 'c': {
                char val = va_arg(ap, int);
                _putchar(val);
                length++;
                break;
            }
            case 's': {
                const char *val = va_arg(ap, const char *);
                length += bios_print_string(val, flags, width);
                break;
            }
#if PRINTF_FULL_PARSE
            case 'o' : {
                /* Octal is rarely used these days */
                uint32_t value = va_arg(ap, uint32_t);
                char *digits = bios_octal_digits(value, temp);
                length += bios_print_digits(0, flags, width, digits);
                break;
            }
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'a':
            case 'A': {
                /* You can choose any floating-point value, as long as it's NaN */
                double val = va_arg(ap, double);
                length += bios_print_string("NaN", flags, width);
                break;
            }
            case 'n': {
                /* Retrieving the length is a bit esoteric */
                uint32_t *ptr = va_arg(ap, uint32_t *);
                *ptr = length;
                break;
            }
#endif
            case '\0':
                return;
            default:
                break;
        }
    } /* while */
}

void
bios_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bios_vprintf(fmt, ap);
    va_end(ap);
}