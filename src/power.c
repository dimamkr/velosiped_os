#include "power.h"
#include "dynamic_array.h"
#include "task.h"
#include "system.h"
#include <acpica/include/acpi.h>


static dynamic_array_t *shutdown_routines = NULL;


void power_invoke_all_shutdown_routines()
{
    TASK_LOCKED_FUNCTION;

    if (shutdown_routines)
        for (uint32_t i = 0;i < shutdown_routines->elements_count;i++)
        {
            power_shutdown_routine_cb routine = dynamic_array_get_by_index(shutdown_routines, i);

            routine();
        }
}

void power_register_shutdown_routine(power_shutdown_routine_cb routine)
{
    TASK_LOCKED_FUNCTION;

    if (shutdown_routines == NULL)
        shutdown_routines = dynamic_array_create(sizeof(power_shutdown_routine_cb));

    dynamic_array_push_back(shutdown_routines, &routine);
}

bool_t power_shutdown()
{
    TASK_LOCKED_FUNCTION;

    if (ACPI_FAILURE(AcpiEnterSleepStatePrep(ACPI_STATE_S5)))
        return false;

    interrupt_disable();

    if (ACPI_FAILURE(AcpiEnterSleepState(ACPI_STATE_S5)))
    {
        interrupt_enable();
        return false;
    }

    return true;
}

bool_t power_gracefully_shutdown()
{
    TASK_LOCKED_FUNCTION;

    power_invoke_all_shutdown_routines();

    return power_shutdown();
}

bool_t power_reboot()
{
    TASK_LOCKED_FUNCTION;

    interrupt_disable();

    // пытаемся сделать reset через порт в FADT ACPI
    AcpiReset();

    // если не получилось сделать reset через ACPI, пытаемся сделать через чипсет
    outb(CHIPSET_PORT, POWER_CHIPSET_INITIATE_RESET);
    outb(CHIPSET_PORT, POWER_CHIPSET_RESET_AND_INITIATE);

    // если не получилось сделать reset через чипсет, пытаемся сделать через контроллер клавиатуры
    outb(KEYBOARD_CONTROLLER_PORT, POWER_KEYBOARD_RESET_VALUE);

    // если не получилось сделать reset через контроллер клавиатуры, возвращаем false
    return false;
}

bool_t power_gracefully_reboot()
{
    TASK_LOCKED_FUNCTION;

    power_invoke_all_shutdown_routines();

    power_reboot();
}