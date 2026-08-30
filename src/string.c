#include "string.h"
#include "bitmap.h"

__attribute__((optimize("O3,unroll-loops")))
uint32_t
strlen(const char *s)
{
    uint32_t len = 0;
    for (; s[len]; len++)
        ;
    return len;
}

__attribute__((optimize("O3,unroll-loops"))) int strcmp(const char *a, const char *b)
{
    for (int i = 0;; ++i)
    {
        if (a[i] == b[i])
        {
            if (a[i] == '\0')
                return 0;
        }
        else
        {
            return a[i] > b[i] ? 1 : -1;
        }
    }
}

char *strdup(const char *src)
{
    uint32_t len = strlen(src);
    char *result = malloc(len + 1);
    result[len] = '\0';
    memcpy(result, src, len);

    return result;
}

void strcat(char *a, const char *b)
{
    uint32_t len_a = strlen(a);
    uint32_t len_b = strlen(b);

    memcpy(a + len_a, b, len_b);
    a[len_a + len_b] = '\0';
}

__attribute__((optimize("O3,unroll-loops"))) void string_to_lower(char *s)
{
    for (; *s; s++)
        *s = LOWER(*s);
}

__attribute__((optimize("O3,unroll-loops"))) void string_to_upper(char *s)
{
    for (; *s; s++)
        *s = UPPER(*s);
}

__attribute__((optimize("O3,unroll-loops")))
uint32_t
strstr(const char *haystack, const char *needle)
{
    // КМП с z-функцией

    uint32_t len_haystack = strlen(haystack);
    uint32_t len_needle = strlen(needle);
    uint32_t len_united = len_haystack + len_needle;

    char *united = malloc(len_haystack + len_needle + 1);
    memcpy(united, needle, len_haystack);
    memcpy(united + len_needle, haystack, len_haystack);
    united[len_haystack + len_needle] = '\0';

    uint32_t *z_func = malloc(sizeof(uint32_t) * len_united);
    z_func[0] = 0;

    uint32_t best_index = 0;

    for (uint32_t i = 1; i < len_united; i++)
    {
        z_func[i] = 0;

        if (i < best_index + z_func[best_index])
        {
            z_func[i] = min(z_func[i - best_index], best_index + z_func[best_index] - i);

            if (i + z_func[i] < best_index + z_func[best_index])
                continue;
        }

        best_index = i;
        for (; i + z_func[i] < len_united && united[i + z_func[i]] == united[z_func[i]]; z_func[i]++)
            ;

        if (z_func[i] >= len_needle)
        {
            free(united);
            free(z_func);

            return i - len_needle;
        }
    }

    free(united);
    free(z_func);

    return -1;
}

__attribute__((optimize("O0")))
bool_t
is_matching_pattern(const char *str, const char *pattern)
{
    uint32_t y_size = strlen(str) + 1;
    uint32_t x_size = strlen(pattern) + 1;

    bitmap_t dp;
    bitmap_t maxes;

    bitmap_init(&dp, x_size * y_size);
    bitmap_set_bit(&dp, x_size * (y_size - 1) + x_size - 1);
    bitmap_init(&maxes, x_size * y_size);
    bitmap_set_bit(&maxes, x_size * (y_size - 1) + x_size - 1);

    for (uint32_t i = y_size - 2; i != -1; i--)
    {
        for (uint32_t j = x_size - 2; j != -1; j--)
        {
            switch (pattern[j])
            {
            case '?':
                if (bitmap_test_bit(&dp, x_size * (i + 1) + (j + 1)))
                    bitmap_set_bit(&dp, x_size * i + j);
                break;
            case '*':
                if (bitmap_test_bit(&dp, x_size * i + (j + 1)) || bitmap_test_bit(&maxes, x_size * (i + 1) + (j + 1)))
                    bitmap_set_bit(&dp, x_size * i + j);
                break;
            default:
                if (str[i] == pattern[j] && bitmap_test_bit(&dp, x_size * (i + 1) + (j + 1)))
                    bitmap_set_bit(&dp, x_size * i + j);
            }

            if (bitmap_test_bit(&dp, x_size * i + (j + 1)) || bitmap_test_bit(&maxes, x_size * (i + 1) + (j + 1)))
                bitmap_set_bit(&maxes, x_size * i + j + 1);
        }
    }

    bool_t result = bitmap_test_bit(&dp, 0);

    bitmap_destroy(&dp);
    bitmap_destroy(&maxes);

    return result;
}

__attribute__((optimize("O3,unroll-loops")))
uint32_t
wide_char_to_utf8(char *dst, const wchar_t *src, uint32_t dst_size)
{
    uint32_t i = 0, j = 0;

    while (src[i] != 0 && j < dst_size - 4)
    {
        if (src[i] <= 0x7F)
        {
            dst[j++] = (char)src[i];
        }
        else if (src[i] <= 0x7FF)
        {
            dst[j++] = 0xC0 | ((src[i] >> 6) & 0x1F);
            dst[j++] = 0x80 | (src[i] & 0x3F);
        }
        else
        {
            dst[j++] = 0xE0 | ((src[i] >> 12) & 0x0F);
            dst[j++] = 0x80 | ((src[i] >> 6) & 0x3F);
            dst[j++] = 0x80 | (src[i] & 0x3F);
        }
        i++;
    }

    dst[j] = '\0';
    return j;
}

void uint32_to_string(uint32_t number, char *result)
{
    uint16_t len = 0;
    for (uint32_t i = number; i != 0; i /= 10)
        len++;
    for (; number != 0; number /= 10)
        result[--len] = '0' + (number % 10);
}

uint32_t string_to_uint32(char *str)
{
    uint32_t result = 0;
    uint32_t str_length = strlen(str);
    uint32_t mul = 1;

    if (str_length == 0)
        return -1;

    for (uint32_t i = str_length - 1; i != -1; i--)
    {
        if (str[i] > '9' || str[i] < '0')
            return -1;

        result += (uint32_t)(str[i] - '0') * mul;
        mul *= 10;
    }

    return result;
}