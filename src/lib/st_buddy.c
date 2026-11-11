#include "st_buddy.h"
#include "system.h"
#include "heap.h"
#include "pow2.h"

static inline void _st_buddy_build(st_buddy_t *this)
{
    for (size_t i = this->tree_size - 1; i >= this->leaves; --i)
        this->tree[i].free_log2p1 = 1;

    for (size_t i = this->leaves - 1; i >= 1; --i)
        this->tree[i].free_log2p1 = this->tree[2 * i].free_log2p1 + 1;

    this->tree[0].free_log2p1 = 0;
}

// адрес начала блока узла
static inline size_t _st_buddy_get_addr(st_buddy_t *this, size_t n)
{
    size_t shift = (size_t)this->depth - log2_floor(n);
    return (n << shift) - this->leaves;
}

// адрес сразу за концом блока узла
static inline size_t _st_buddy_get_end(st_buddy_t *this, size_t n)
{
    size_t shift = (size_t)this->depth - log2_floor(n);
    return ((n + 1) << shift) - this->leaves;
}

st_buddy_t *st_buddy_create(size_t depth)
{
    st_buddy_t *res = malloc(sizeof(st_buddy_t));
    st_buddy_init(res, depth);
    return res;
}

void st_buddy_init(st_buddy_t *this, size_t depth)
{
    this->depth = depth;
    this->leaves = (size_t)1 << depth;
    this->tree_size = this->leaves * 2;
    this->tree = (st_buddy_node_t *)malloc(this->tree_size * sizeof(st_buddy_node_t));
    this->owns_tree = true;

    _st_buddy_build(this);
}

// buff >= 2 * (1 << depth) * sizeof(st_buddy_node_t) байт
void st_buddy_init_from_buff(st_buddy_t *this, void *buff, size_t depth)
{
    this->depth = depth;
    this->leaves = (size_t)1 << depth;
    this->tree_size = this->leaves * 2;
    this->tree = (st_buddy_node_t *)buff;
    this->owns_tree = false;

    _st_buddy_build(this);
}

void st_buddy_destroy(st_buddy_t *this)
{
    if (this->owns_tree)
        free(this->tree);
}

// size округляется вверх до степени двойки
size_t st_buddy_alloc(st_buddy_t *this, size_t size)
{
    if (size == 0 || size > this->leaves)
        return ST_BUDDY_ERROR;

    uint8_t f_size = (uint8_t)(log2_ceil(size) + 1);

    if (this->tree[1].free_log2p1 < f_size)
        return ST_BUDDY_ERROR;

    size_t n = 1;
    while (n < this->leaves)
    {
        if (this->tree[2 * n].free_log2p1 >= f_size)
            n = 2 * n;
        else if (this->tree[2 * n + 1].free_log2p1 >= f_size)
            n = 2 * n + 1;
        else
            break; // n — искомый свободный блок
    }

    if (this->tree[n].free_log2p1 != f_size)
        return ST_BUDDY_ERROR;

    size_t addr = _st_buddy_get_addr(this, n);
    this->tree[n].free_log2p1 = 0;

    for (n /= 2; n; n /= 2)
        this->tree[n].free_log2p1 = max2(this->tree[2 * n].free_log2p1,
                                         this->tree[2 * n + 1].free_log2p1);

    return addr;
}

// addr должен быть строго началом выделенного блока
bool_t st_buddy_free(st_buddy_t *this, size_t addr)
{
    if (addr >= this->leaves)
        return false;

    size_t n = addr + this->leaves;
    size_t f_vol = 1; // "размер" узла который будет освобожден

    // ближайший узел с f == 0 — выделенный блок (или n == 0 при неверной попытке освобождения)
    while (this->tree[n].free_log2p1 != 0)
    {
        n /= 2;
        f_vol++;
    }

    if (unlikely(n == 0 || addr != _st_buddy_get_addr(this, n)))
        return false;

    this->tree[n].free_log2p1 = (uint8_t)f_vol;
    n /= 2;
    f_vol++;

    while (n)
    {
        uint8_t child_vol = (uint8_t)(f_vol - 1); // "размер" полностью свободного узла ребенка
        if (this->tree[2 * n].free_log2p1 == child_vol &&
            this->tree[2 * n + 1].free_log2p1 == child_vol)
            this->tree[n].free_log2p1 = (uint8_t)f_vol;
        else
            this->tree[n].free_log2p1 = max2(this->tree[2 * n].free_log2p1,
                                             this->tree[2 * n + 1].free_log2p1);

        n /= 2;
        f_vol++;
    }

    return true;
}

bool_t st_buddy_is_free(st_buddy_t *this, size_t addr)
{
    if (addr >= this->leaves)
        return false;

    for (size_t n = addr + this->leaves; n; n /= 2)
        if (this->tree[n].free_log2p1 == 0)
            return false;

    return true;
}

// внутри ли отрезка
static inline bool_t _is_node_in_interval(st_buddy_t *this, size_t n, size_t l, size_t r)
{
    return _st_buddy_get_addr(this, n) >= l && (_st_buddy_get_end(this, n) - 1) <= r;
}

// пометить отрезок [laddr, laddr + length) занятым
// ВАЖНО: отрезок строго полностью свободен и его потом не получится нормально освободить
bool_t st_buddy_set_alloced(st_buddy_t *this, size_t laddr, size_t length)
{
    if (length == 0 || laddr >= this->leaves || length > this->leaves - laddr)
        return false;

    const size_t raddr = laddr + length - 1;
    const size_t l_leaf = this->leaves + laddr;
    const size_t r_leaf = this->leaves + raddr;

    size_t n = l_leaf;

    while (1)
    {
        // подняться до наибольшего узла, содержащегося в [laddr, raddr]
        size_t n_tmp = n;
        while (_is_node_in_interval(this, n_tmp, laddr, raddr))
        {
            n = n_tmp;
            n_tmp /= 2;
        }

        // пока узел вылезает за границы — спуститься к левому ребенку
        while (!_is_node_in_interval(this, n, laddr, raddr))
        {
            n = n * 2;
        }

        this->tree[n].free_log2p1 = 0;

        // рассмотренные узлы покрыли весь отрезок
        if (_st_buddy_get_end(this, n) == raddr + 1)
            break;

        ++n;
    }

    // обновление зависимых узлов дерева (достаточно обновить только предки l_leaf и r_leaf)

    n = l_leaf / 2;
    while (n)
    {
        this->tree[n].free_log2p1 = max2(this->tree[2 * n].free_log2p1,
                                         this->tree[2 * n + 1].free_log2p1);
        n /= 2;
    }

    n = r_leaf / 2;
    while (n)
    {
        this->tree[n].free_log2p1 = max2(this->tree[2 * n].free_log2p1,
                                         this->tree[2 * n + 1].free_log2p1);
        n /= 2;
    }

    return true;
}