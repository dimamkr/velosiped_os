#ifndef IDT_INITIALISER
#define IDT_INITIALISER

#include "types.h"

// Describes one IDT entry
typedef struct
{
    uint16_t base_low;  // the lower 16 bits of the address to jump on interrupt
    uint16_t selector;  // kernel segment selector
    uint8_t zero;       // always zero
    uint8_t flags;      // more flags
    uint16_t base_high; // the upper 16 bits of the address to jump on interrupt
} __attribute__((packed)) idt_entry_t;

// Describes a pointer to the IDT
typedef struct
{
    uint16_t limit;
    uint32_t base; // the address of the first IDT entry
} __attribute__((packed)) idt_ptr_t;

void idt_init(void);

// ISR handlers
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

// IRQ handlers (ISR 32-47)
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

#endif