#include "acpi.h"
#include "heap.h"
#include "ram.h"
#include "vmm.h"
#include "syncapi.h"
#include "task.h"
#include "timer.h"
#include "pci.h"
#include "konsole.h"

#define ACPI_USE_CUSTOM_CACHE

/*
 * OSL Initialization and shutdown primitives
 */

ACPI_STATUS AcpiOsInitialize(void)
{
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate(void)
{
    return AE_OK;
}

/*
 * ACPI Table interfaces
 */

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void)
{
    // сканируем адреса, ищем таблицу
    // диапазон 0x000E0000-0x000FFFFF, выравнивание 16 байт

    void *start_scan_addr = ram_kernel_to_virt((void*)0x000E0000);
    void *end_scan_addr = start_scan_addr + 131072;

    for (void *cur = start_scan_addr;cur < end_scan_addr;cur += 16)
    {
        if (memcmp(cur, "RSD PTR ", 8))
        {
            uint8_t checksum = 0;

            for (void *cur1 = cur;cur1 < cur + 20;cur1++)
                checksum += *(byte_t*)cur1;

            if (checksum == 0)
                return (ACPI_PHYSICAL_ADDRESS)(uint32_t)ram_kernel_to_phys(cur);
        }
    }

    return 0;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal, ACPI_STRING *NewVal)
{
    *NewVal = NULL;

    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
    *NewTable = NULL;

    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
    *NewAddress = 0;
    *NewTableLength = 0;

    return AE_OK;
}

/*
 * Spinlock primitives
 */

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
    *OutHandle = semaphore_create(1);

    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
    semaphore_destroy(Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    semaphore_acquire(Handle, 1, SYNC_TIMEOUT_INFINITE);
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    semaphore_release(Handle, 1);
}

/*
 * Semaphore primitives
 */

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle)
{
    if (InitialUnits > MaxUnits)
        return AE_BAD_PARAMETER;

    *OutHandle = semaphore_create(MaxUnits);
    
    if (semaphore_acquire(*OutHandle, InitialUnits, SYNC_TIMEOUT_INFINITE))
        return AE_OK;

    semaphore_destroy(*OutHandle);

    return AE_ERROR;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    semaphore_destroy(Handle);

    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    if (semaphore_acquire(Handle, Units, Timeout))
        return AE_OK;

    return AE_TIME;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    semaphore_release(Handle, Units);

    return AE_OK;
}

/*
 * Memory allocation and mapping
 */

void *AcpiOsAllocate(ACPI_SIZE Size)
{
    return malloc(Size);
}

/*
void *AcpiOsAllocateZeroed(ACPI_SIZE Size)
{
    void *result = malloc(Size);

    if (result == NULL)
        return false;

    memset(result, 0, Size);

    return result;
}
*/

void AcpiOsFree(void *Memory)
{
    free(Memory);
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
    if (Where + Length < 64*MB)
        return ram_kernel_to_virt((void*)(uint32_t)Where);
    return vmm_map_mmio((uint32_t)Where, Length / 4096 + ((Length % 4096) > 0));
}
                   
void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size)
{
    // NOT IMPLEMENTED
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
    *PhysicalAddress = (ACPI_PHYSICAL_ADDRESS)(uint32_t)ram_kernel_to_phys(LogicalAddress);

    return AE_OK;
}

/*
 * Memory/Object Cache
 */

#ifdef ACPI_USE_CUSTOM_CACHE
 
struct ACPI_CACHE {
    uint16_t object_size;
    uint16_t max_depth;            
    void *buffer;
    dynamic_array_t *free_blocks;
};

ACPI_STATUS AcpiOsCreateCache(char *CacheName, UINT16 ObjectSize, UINT16 MaxDepth, ACPI_CACHE_T **ReturnCache)
{
    struct ACPI_CACHE *result = malloc(sizeof(struct ACPI_CACHE));
    result->object_size = (uint32_t)ObjectSize;
    result->max_depth = MaxDepth;
    result->buffer = malloc((uint32_t)MaxDepth * (uint32_t)ObjectSize);
    result->free_blocks = dynamic_array_create(2);

    for (uint16_t i = 0;i < MaxDepth;i++)
        dynamic_array_push_back(result->free_blocks, &i);

    *ReturnCache = (ACPI_CACHE_T*)result;

    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteCache(ACPI_CACHE_T *Cache)
{
    struct ACPI_CACHE *cache = (struct ACPI_CACHE*)Cache;

    free(cache->buffer);
    dynamic_array_destroy(cache->free_blocks);
    free(Cache);

    return AE_OK;
}

ACPI_STATUS AcpiOsPurgeCache(ACPI_CACHE_T *Cache)
{
    struct ACPI_CACHE *cache = (struct ACPI_CACHE*)Cache;

    dynamic_array_clear(cache->free_blocks);

    return AE_OK;
}

void *AcpiOsAcquireObject(ACPI_CACHE_T *Cache)
{
    struct ACPI_CACHE *cache = (struct ACPI_CACHE*)Cache;

    if (cache->free_blocks->elements_count == 0)
        return NULL;

    void *result = cache->buffer + (uint32_t)cache->object_size * (*(uint16_t*)dynamic_array_get_bottom(cache->free_blocks));
    dynamic_array_pop_front(cache->free_blocks);

    return result;
}

ACPI_STATUS AcpiOsReleaseObject (ACPI_CACHE_T *Cache, void *Object)
{
    struct ACPI_CACHE *cache = (struct ACPI_CACHE*)Cache;

    uint16_t index = (uint32_t)(Object - cache->buffer) / cache->object_size;

    dynamic_array_push_back(cache->free_blocks, &index);

    return AE_OK;
}

#endif

/*
 * Interrupt handlers
 */

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context)
{
    return AE_OK; // NOT IMPLEMENTED
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine)
{
    return AE_OK; // NOT IMPLEMENTED
}

/*
 * Threads and Scheduling
 */

uint32_t threads_to_wait = 0; 

ACPI_THREAD_ID AcpiOsGetThreadId()
{
    return current_task->pid;
}

void AcpiThreadWrapper(void *_param) // какой-т костыль
{
    PAIR (ACPI_OSD_EXEC_CALLBACK, void*) *param = _param;

    (param->first)(param->second);

    free(param);
    __sync_fetch_and_sub(&threads_to_wait, 1);
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
    __sync_fetch_and_add(&threads_to_wait, 1);

    PAIR (ACPI_OSD_EXEC_CALLBACK, void*) *param = malloc(sizeof(PAIR (ACPI_OSD_EXEC_CALLBACK, void*)));
    param->first = Function; // лютый костыль
    param->second = Context;

    task_create(AcpiThreadWrapper, param, STACK_SIZE_LARGE);

    return AE_OK;
}

void AcpiOsWaitEventsComplete(void)
{
    while (threads_to_wait > 0)
        task_yield();
}

void AcpiOsSleep(UINT64 Milliseconds)
{
    timer_wait(Milliseconds);
}

void AcpiOsStall(UINT32 Microseconds)
{
    task_lock();

    Microseconds *= 1000; // грубый костыль

    while (Microseconds--)
        asm("pause");

    task_unlock();
}

/*
 * Platform and hardware-independent I/O interfaces
 */

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width)
{
    switch (Width)
    {
        case 8:  
            *Value = inb(Address);
            return AE_OK;
        case 16:
            *Value = inw(Address);
            return AE_OK;
        case 32:
            *Value = inl(Address);
            return AE_OK;
        default:
            return AE_BAD_PARAMETER;
    }
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    switch (Width)
    {
        case 8:
            outb(Address, (uint8_t)Value);
            return AE_OK;
        case 16:
            outw(Address, (uint16_t)Value);
            return AE_OK;
        case 32:
            outl(Address, (uint32_t)Value);
            return AE_OK;
        default:
            return AE_BAD_PARAMETER;
    }
}

/*
 * Platform and hardware-independent physical memory interfaces
 */

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width)
{
    void *addr = vmm_map_mmio(Address, 1);

    switch (Width)
    {
        case 8:
            *Value = *(uint8_t*)addr;
            return AE_OK;
        case 16:
            *Value = *(uint16_t*)addr;
            return AE_OK;
        case 32:
            *Value = *(uint32_t*)addr;
            return AE_OK;
        default:
            return AE_BAD_PARAMETER;
    }
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
    void *addr = vmm_map_mmio(Address, 1);

    switch (Width)
    {
        case 8:
            *(uint8_t*)addr = (uint8_t)Value;
            return AE_OK;
        case 16:
            *(uint16_t*)addr = (uint16_t)Value;
            return AE_OK;
        case 32:
            *(uint32_t*)addr = (uint32_t)Value;
            return AE_OK;
        default:
            return AE_BAD_PARAMETER;
    }
}

/*
 * Platform and hardware-independent PCI configuration space access
 * Note: Can't use "Register" as a parameter, changed to "Reg" --
 * certain compilers complain.
 */

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width)
{
    uint32_t raw = pci_read(PCI_MAKE_DEVICE_OFFSET(PciId->Bus, PciId->Device, PciId->Function), Reg & 0xFC);
    
    switch (Width)
    {
        case 8:
            *Value = (raw >> ((Reg & 3) * 8)) & 0xFF;
            return AE_OK;
        case 16:
            *Value = (raw >> ((Reg & 2) * 8)) & 0xFFFF;
            return AE_OK;
        case 32:
            *Value = raw;
            return AE_OK;
        default:
            return AE_BAD_PARAMETER;
    }
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width)
{
    if (Width < 32)
    {
        uint32_t old = pci_read(PCI_MAKE_DEVICE_OFFSET(PciId->Bus, PciId->Device, PciId->Function), Reg & 0xFC);
        uint32_t new;
        uint32_t shift = (Reg & 3) * 8;
        uint32_t mask = (Width == 8) ? 0xFF : 0xFFFF;
        
        new = (old & ~(mask << shift)) | ((uint32_t)Value << shift);
        pci_write(PCI_MAKE_DEVICE_OFFSET(PciId->Bus, PciId->Device, PciId->Function), Reg & 0xFC, new);
    }
    else
        pci_write(PCI_MAKE_DEVICE_OFFSET(PciId->Bus, PciId->Device, PciId->Function), Reg & 0xFC, (uint32_t)Value);

    return AE_OK;
}

/*
 * Miscellaneous
 */

BOOLEAN AcpiOsReadable(void *Pointer, ACPI_SIZE Length)
{
    return true; // у нас любая память читаемая, пока нет защиты
}

BOOLEAN AcpiOsWritable(void *Pointer, ACPI_SIZE Length)
{
    return true; // у нас любая память записываемая, пока нет защиты
}

UINT64 AcpiOsGetTimer()
{
    return timer_get_time();
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info)
{
    if (Function == ACPI_SIGNAL_FATAL)
    {
        ACPI_SIGNAL_FATAL_INFO *fatal = Info;

        konsole_printf("Fatal ACPI error, type=0x%x, code=0x%x", fatal->Type, fatal->Code);
        PANIC("ACPI FATAL ERROR");
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep (UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue)
{
    // не используется
    return AE_OK;
}

/*
 * Debug print routines
 */

ACPI_PRINTF_LIKE (1)
void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char *Format, ...)
{
    va_list args;
    va_start(args, Format);

    konsole_print(Format);
    //konsole_vprintf(Format, args);
    
    va_end(args);
}

void AcpiOsVprintf(const char *Format, va_list Args)
{
    konsole_print(Format);
    //konsole_vprintf(Format, Args);
}

void AcpiOsRedirectOutput(void *Destination)
{
    // NOT IMPLEMENTED
}

/*
 * Debug IO
 */

ACPI_STATUS AcpiOsGetLine(char *Buffer, UINT32 BufferLength, UINT32 *BytesRead)
{
    return AE_NOT_IMPLEMENTED;
}

ACPI_STATUS AcpiOsInitializeDebugger(void)
{
    return AE_NOT_IMPLEMENTED;
}

void AcpiOsTerminateDebugger(void)
{
    // NOT IMPLEMENTED
}

ACPI_STATUS AcpiOsWaitCommandReady(void)
{
    return AE_NOT_IMPLEMENTED;
}

ACPI_STATUS AcpiOsNotifyCommandComplete(void)
{
    return AE_NOT_IMPLEMENTED;
}

void AcpiOsTracePoint(ACPI_TRACE_EVENT_TYPE Type, BOOLEAN Begin, UINT8 *Aml, char *Pathname)
{
    // NOT IMPLEMENTED
}

/*
 * Obtain ACPI table(s)
 */

ACPI_STATUS AcpiOsGetTableByName(char *Signature, UINT32 Instance, ACPI_TABLE_HEADER **Table, ACPI_PHYSICAL_ADDRESS *Address)
{
    return AE_NOT_IMPLEMENTED;
}

ACPI_STATUS AcpiOsGetTableByIndex(UINT32 Index, ACPI_TABLE_HEADER **Table, UINT32 *Instance, ACPI_PHYSICAL_ADDRESS *Address)
{
    return AE_NOT_IMPLEMENTED;
}

ACPI_STATUS AcpiOsGetTableByAddress(ACPI_PHYSICAL_ADDRESS Address, ACPI_TABLE_HEADER **Table)
{
    return AE_NOT_IMPLEMENTED;if (Table)
        *Table = NULL;

    return AE_NOT_FOUND;
}

/*
 * Directory manipulation
 */

void *AcpiOsOpenDirectory(char *Pathname, char *WildcardSpec, char RequestedFileType)
{
    return NULL; // NOT IMPLEMENTED
}

char *AcpiOsGetNextFilename(void *DirHandle)
{
    return NULL; // NOT IMPLEMENTED
}

void AcpiOsCloseDirectory (void *DirHandle)
{
    // NOT IMPLEMENTED
}