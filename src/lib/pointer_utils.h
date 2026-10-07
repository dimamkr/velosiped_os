#ifndef PTR_UTILS
#define PTR_UTILS

// СОГЛАШЕНИЕ О ВЫСОКОУРОВНЕВЫХ СТРУКТУРАХ ДАННЫХ
// тип данных type_t
// type_destroy - деструктор

// макрос объявление функции уничтожения (в .h файл структуры)
#define AUTOCLEANUP_DEFINE_FUNC(type_without_t)                                                            \
    static inline void __attribute__((always_inline)) __cleanup_##type_without_t(type_without_t##_t **ptr) \
    {                                                                                                      \
        if (*ptr)                                                                                          \
        {                                                                                                  \
            type_without_t##_destroy(*ptr);                                                                \
            *ptr = NULL;                                                                                   \
        }                                                                                                  \
    }

// макрос авточистки высокоуровневых структур
#define UNIQUE_PTR(type_without_t) \
    __attribute__((cleanup(__cleanup_##type_without_t))) type_without_t##_t *

#include "heap.h"
//--------------------------------------------------------------------------------
static inline __attribute__((always_inline)) void __cleanup_4_autofree_ptr(void *_ptr)
{
    void **ptr = (void **)_ptr;
    if ((uint32_t)*ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

// для обычной очистки (без деструктора)
#define AUTOFREE_PTR(type) \
    __attribute__((cleanup(__cleanup_4_autofree_ptr))) type *

#endif
