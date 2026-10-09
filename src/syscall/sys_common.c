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
#include "sys_file.h"
#include "sys_mem.h"
#include "sys_proc.h"
#include "sys_time.h"

extern task_t tasks[];

static inline void _destroy_all_wins_with_owner(uint32_t pid);

void syscall_cleanup_task(uint32_t pid)
{
    TASK_LOCKED_FUNCTION;
    // окна
    _destroy_all_wins_with_owner(pid);
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

// TODO (несложно) поддержка канала мыши
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

    // есть права на чтение
    uint32_t sid = current_task->input_sids[fd];
    if (sid == TASK_INPUT_NONE)
        return -1;

    // чтение блокирующее процесс
    keyboard_event_t ev;
    while (!task_input_get(fd, &ev))
    {
        task_wait_for_input(fd);
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
    syscall_cleanup_task(current_task->pid);
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
    TASK_LOCKED_FUNCTION;
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