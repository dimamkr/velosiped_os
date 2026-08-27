#include "bitmap.h"
#include "system.h"

// инварианты
// внутри uint32_t индексация начиная м младшего бита (как в little endian)

void bitmap_init(bitmap_t *this, uint32_t *buff, uint32_t bits_count)
{
    this->buff = buff;
    this->bits_count = bits_count;
    this->blocks_count = (bits_count + REM) >> 5;

    memset(this->buff, 0, this->blocks_count * sizeof(uint32_t));
}

uint32_t bitmap_find_first_zero(bitmap_t *this, uint32_t start_index)
{
    for (; (start_index & REM) != 0 && start_index < this->bits_count; ++start_index)
    {
        if (!bitmap_test_bit(this, start_index))
        {
            return start_index;
        }
    }

    for (; start_index < (this->bits_count & ~REM); start_index += BLC)
    {
        if (~this->buff[start_index >> 5])
        {
            return start_index + __builtin_ctz(~(this->buff[start_index >> 5]));
        }
    }

    for (; start_index < this->bits_count; ++start_index)
    {
        if (!bitmap_test_bit(this, start_index))
        {
            return start_index;
        }
    }

    return this->bits_count;
}

uint32_t bitmap_find_first_one(bitmap_t *this, uint32_t start_index)
{
    for (; (start_index & REM) != 0 && start_index < this->bits_count; ++start_index)
    {
        if (bitmap_test_bit(this, start_index))
        {
            return start_index;
        }
    }

    for (; start_index < (this->bits_count & ~REM); start_index += BLC)
    {
        if (this->buff[start_index >> 5])
        {
            return start_index + __builtin_ctz(this->buff[start_index >> 5]);
        }
    }

    for (; start_index < this->bits_count; ++start_index)
    {
        if (bitmap_test_bit(this, start_index))
        {
            return start_index;
        }
    }

    return this->bits_count;
}

// заполнение [start_index,end_index)
void bitmap_set_interval(bitmap_t *this, uint32_t start_index, uint32_t end_index)
{
    for (; (start_index & REM) != 0 && start_index < end_index; ++start_index)
    {
        bitmap_set_bit(this, start_index);
    }

    for (; start_index < (end_index & ~REM); start_index += BLC)
    {
        this->buff[start_index >> 5] = (uint32_t)-1;
    }

    for (; start_index < end_index; ++start_index)
    {
        bitmap_set_bit(this, start_index);
    }
}

// очистка [start_index,end_index)
void bitmap_clear_interval(bitmap_t *this, uint32_t start_index, uint32_t end_index)
{
    for (; (start_index & REM) != 0 && start_index < end_index; ++start_index)
    {
        bitmap_clear_bit(this, start_index);
    }

    for (; start_index < (end_index & ~REM); start_index += BLC)
    {
        this->buff[start_index >> 5] = (uint32_t)0;
    }

    for (; start_index < end_index; ++start_index)
    {
        bitmap_clear_bit(this, start_index);
    }
}

// заполненяет 1 первый найденный интервал и возвращает индекс начала
uint32_t bitmap_alloc_interval(bitmap_t *this, uint32_t length)
{
    uint32_t start_index = 0;

    while (start_index < this->bits_count)
    {
        start_index = bitmap_find_first_zero(this, start_index);
        uint32_t end_index = bitmap_find_first_one(this, start_index);

        if (end_index - start_index >= length)
        {
            bitmap_set_interval(this, start_index, start_index + length);
            return start_index;
        }

        start_index = end_index;
    }

    return this->bits_count;
}