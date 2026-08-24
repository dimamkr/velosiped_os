#include "isr.h"
#include "pic.h"
#include "konsole.h"
#include "types.h"
#include "system.h"

// Number of spurious interrupts received
uint64_t spurious_interrupts;

isr_t interruption_top_handlers[256] = {NULL};
isr_t interruption_bottom_handlers[256] = {NULL};

static uint16_t get_pic_isr(void)
{
    // Read the In-Service Register (ISR) of the PICs.
    // This is done by first writing the OCW3 command word
    // to the PIC command port, then immediately reading from
    // *command* (not data) port.
    outb(PIC1_CMD, PIC_READ_ISR); // master
    outb(PIC2_CMD, PIC_READ_ISR); // slave
    return (inb(PIC2_CMD) << 8) | inb(PIC1_CMD);
}

static bool_t is_spurious(isr_data_t registers)
{
    uint16_t isr = get_pic_isr();
    if (registers.int_no == IRQ7)
    {
        // Check the master PIC ISR
        return (!(0x00FF & isr));
    }
    else if (registers.int_no == IRQ15)
    {
        // Check the slave PIC ISR
        return (!(0xFF00 & isr));
    }

    return false;
}

static void invoke_top_handler(isr_data_t registers)
{
    if (interruption_top_handlers[registers.int_no] != NULL)
    {
        isr_t top_handler = interruption_top_handlers[registers.int_no];
        top_handler(registers);
    }
    else
    {
        konsole_printf("%s%d\n", "Unhandled interrupt: ", registers.int_no);
    }
    // else
    // {
    //     konsole_println("unhandled interrupt: ");
    //     // konsole_printlni(registers.int_no, 'd');
    //     if (IRQ0 <= registers.int_no && registers.int_no <= IRQ15)
    //     {
    //         konsole_println(" (IRQ");
    //         // konsole_printlni(registers.int_no - IRQ0, 'd');
    //         konsole_println(")");
    //     }
    //     konsole_println("");
    //     // PANIC("unhandled interrupt");
    // }
}

void invoke_bottom_handler(isr_data_t registers)
{
    if (interruption_bottom_handlers[registers.int_no] != NULL)
    {
        isr_t bottom_handler = interruption_bottom_handlers[registers.int_no];
        bottom_handler(registers);
    }
}

void isr_handler(isr_data_t registers)
{
    invoke_top_handler(registers);
}

void irq_handler(isr_data_t registers)
{
    // Noise on the IRQ or INTR lines, or software sending
    // an EOI at the wrong time can lead to spurious interrupts.
    // We must check for spurious interrupts here so we can
    // ignore them and *NOT* send an EOI.
    if (is_spurious(registers))
    {
        spurious_interrupts++;
        konsole_print("spurious interrupt: IRQ");
        // konsole_print(registers.int_no - IRQ0, 'd');
        return;
    }

    // Send an EOI (end of interrupt) signal to the PICs
    if (registers.int_no >= IRQ8)
    {
        // Send reset signal to the slave
        outb(PIC2_CMD, PIC_EOI);
    }
    // Send reset signal to the master
    outb(PIC1_CMD, PIC_EOI);

    invoke_top_handler(registers);
}

void interrupt_register(uint8_t n, isr_t top_handler, isr_t bottom_handler)
{
    interruption_top_handlers[n] = top_handler;
    interruption_bottom_handlers[n] = bottom_handler;
}

void interrupt_enable()
{
    asm volatile("sti");
}

void interrupt_disable()
{
    asm volatile("cli");
}