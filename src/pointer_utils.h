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

// макрос авточистки который нужно писать вместо объявления указателя
#define AUTOCLEANUP_PTR(type_without_t) \
    __attribute__((cleanup(__cleanup_##type_without_t))) type_without_t##_t *
