#include "_syscall_func.h"
#include "task.h"
#include "konsole.h"
#include "fat32.h"
#include "vmm.h"
#include "input_manager.h"
#include "string.h"
#include "ram.h"
#include "keyboard.h"
#include "composer.h"

#define _CONCAT_UNFOLD(a, b) a##b
#define CONCAT(a, b) _CONCAT_UNFOLD(a, b)

#define _DECL_VAR(n) uint32_t _##n
#define _UNUSD_VAR(n) (void)_##n;

#define _DECL_CHAIN_1 _DECL_VAR(1)
#define _DECL_CHAIN_2 _DECL_CHAIN_1, _DECL_VAR(2)
#define _DECL_CHAIN_3 _DECL_CHAIN_2, _DECL_VAR(3)
#define _DECL_CHAIN_4 _DECL_CHAIN_3, _DECL_VAR(4)
#define _DECL_CHAIN_5 _DECL_CHAIN_4, _DECL_VAR(5)
#define _DECL_CHAIN_6 _DECL_CHAIN_5, _DECL_VAR(6)

#define _UNUSD_CHAIN_1 _UNUSD_VAR(1)
#define _UNUSD_CHAIN_2 _UNUSD_CHAIN_1 _UNUSD_VAR(2)
#define _UNUSD_CHAIN_3 _UNUSD_CHAIN_2 _UNUSD_VAR(3)
#define _UNUSD_CHAIN_4 _UNUSD_CHAIN_3 _UNUSD_VAR(4)
#define _UNUSD_CHAIN_5 _UNUSD_CHAIN_4 _UNUSD_VAR(5)
#define _UNUSD_CHAIN_6 _UNUSD_CHAIN_5 _UNUSD_VAR(6)

#define DECL_ARGS(n) CONCAT(_DECL_CHAIN_, n)
#define UNUSD_ARGS(n) CONCAT(_UNUSD_CHAIN_, n)

// ИНВАРИАНТ ВСЕ ФУНКЦИИ ВОЗВРАЩАЮТ ЛИБО 0 либо запрошенное значение при успехе; ПРИ НЕУСПЕХЕ -1

static inline void _destroy_all_wins_with_owner(uint32_t pid);

// простейшая проверка пользовательского указателя
static bool_t _user_ptr_ok(uint32_t ptr, uint32_t size)
{
    if (ptr == 0)
        return false;
    if (ptr >= RAM_VIRTUAL_START)
        return false;
    if (ptr + size < ptr)
        return false;
    if (ptr + size > RAM_VIRTUAL_START)
        return false;
    return true;
}

int sys_write(uint32_t fd, const char *buff, uint32_t size, DECL_ARGS(3))
{
    UNUSD_ARGS(3)

    if (fd != 1 && fd != 2) // пока поддерживаем только stdout/stderr
        return -1;

    if (!_user_ptr_ok((uint32_t)buff, size))
        return -1;

    // konsole_println("W");
    for (uint32_t i = 0; i < size; i++)
        konsole_putch(buff[i]);

    return size;
}

int sys_read(uint32_t fd, void *buff, uint32_t size, DECL_ARGS(3))
{
    UNUSD_ARGS(3)
    // Пока поддерживаем только keyboard канал.
    if (fd != 0)
        return -1;

    if (size < sizeof(keyboard_event_t))
        return -1;

    if (!_user_ptr_ok((uint32_t)buff, sizeof(keyboard_event_t)))
        return -1;

    // чтение блокирующее процесс
    keyboard_event_t ev;
    while (!task_input_get(INPUT_CHANNEL_KEYBOARD, &ev))
    {
        task_wait_for_input(INPUT_CHANNEL_KEYBOARD);
    }

    // konsole_printf(" keycode %d pressed %d\n", ev.keycode, ev.pressed);

    memcpy(buff, &ev, sizeof(ev));
    return sizeof(ev);
}

// ms — сколько миллисекунд спать
int sys_sleep(uint32_t ms, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    task_sleep(ms);
    return 0;
}

// Проверить, есть ли данные на канале.
// Возвращает 1, если есть, 0 если нет.
int sys_poll(uint32_t fd, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    if (fd >= INPUT_CHANNELS)
        return 0;

    uint32_t sid = current_task->input_sids[fd];
    if (sid == TASK_INPUT_NONE)
        return 0;

    return !input_manager_queue_empty(fd, sid);
}

int sys_exit(uint32_t status, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    (void)status;
    _destroy_all_wins_with_owner(current_task->pid);
    task_exit();
    return 0; // недостижимо
}

// -------------------------------------------------------
// заглушка для рисования пользовательских окон (TODO: в будущем перенести в user process)
// -------------------------------------------------------
#define MAX_WINDOWS 16
typedef struct
{
    bool_t active;
    uint32_t owner_pid;
    layer_t *layer;
} window_t;

static window_t windows[MAX_WINDOWS];

static inline bool_t _window_ok(uint32_t idx)
{
    if (idx >= MAX_WINDOWS || !windows[idx].active)
        return false;
    if (windows[idx].owner_pid != current_task->pid)
        return false;

    return true;
}

static inline void _window_destroy(int idx)
{
    composer_erase_layer(windows[idx].layer);
    windows[idx].active = false;
}

static inline void _destroy_all_wins_with_owner(uint32_t pid)
{
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        if (windows[i].active && windows[i].owner_pid == pid)
        {
            _window_destroy(i);
        }
    }
}

// создать окно вернуть номер
int sys_win_create(uint32_t w, uint32_t h, int32_t x, int32_t y, int32_t z, DECL_ARGS(1))
{
    UNUSD_ARGS(1)
    if (w == 0 || h == 0 || w > 4096 || h > 4096)
        return -1;

    // найти свободный слот
    int idx = -1;
    for (int i = 0; i < MAX_WINDOWS; ++i)
    {
        if (!windows[i].active)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return -1;

    // создать слой
    layer_t *l = composer_create_layer(x, y, w, h, z,
                                       LAYER_VISIBLE | LAYER_OPAQUE);
    if (!l)
        return -1;

    windows[idx].active = true;
    windows[idx].owner_pid = current_task->pid;
    windows[idx].layer = l;

    return idx;
}

int sys_win_commit(uint32_t idx, void *usr_buf, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    if (!_window_ok(idx))
        return -1;

    window_t *w = &windows[idx];
    uint32_t size = w->layer->x_len * w->layer->y_len * sizeof(uint32_t);

    if (!_user_ptr_ok((uint32_t)usr_buf, size))
        return -1;

    // копируем в буффер для редактирования
    LAYER_EDIT_FUNCTION(w->layer);
    memcpy_xl(w->layer->buff2, usr_buf, size); // бесполезные накладные расходы (2 копирования вместо одного до рисования)

    return 0;
}

// переместить окно
int sys_win_move(uint32_t idx, int32_t x, int32_t y, DECL_ARGS(3))
{
    UNUSD_ARGS(3)
    if (!_window_ok(idx))
        return -1;

    layer_move(windows[idx].layer, x, y);
    return 0;
}

// Изменить z-index
int sys_win_set_z(uint32_t idx, int32_t z, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    if (!_window_ok(idx))
        return -1;

    layer_set_z_index(windows[idx].layer, z);
    return 0;
}

// Закрыть окно
int sys_win_destroy(uint32_t idx, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    if (!_window_ok(idx))
        return -1;

    composer_erase_layer(windows[idx].layer);
    windows[idx].active = false;

    return 0;
}

syscall_fn syscall6_table[SYSCALL6_TABLE_COUNT] = {
    [SYS_READ] = (syscall_fn)sys_read,
    [SYS_WRITE] = (syscall_fn)sys_write,
    // [SYS_OPEN] = (syscall_fn)sys_open,
    // [SYS_CLOSE] = (syscall_fn)sys_close,
    [SYS_EXIT] = (syscall_fn)sys_exit,
    [SYS_SLEEP] = (syscall_fn)sys_sleep,
    [SYS_POLL] = (syscall_fn)sys_poll,
    // оконные
    [SYS_WIN_CREATE] = (syscall_fn)sys_win_create,
    [SYS_WIN_COMMIT] = (syscall_fn)sys_win_commit,
    [SYS_WIN_MOVE] = (syscall_fn)sys_win_move,
    [SYS_WIN_SET_Z] = (syscall_fn)sys_win_set_z,
    [SYS_WIN_DESTROY] = (syscall_fn)sys_win_destroy,
};
