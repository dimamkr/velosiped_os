#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

// VBE ModeInfoBlock (VBE 2.0+)
// Соответствует стандарту VESA BIOS Extension, глава 4.2
typedef struct
{
    uint16_t mode_attributes;    // 0x00  ModeAttributes
    uint8_t win_a_attributes;    // 0x02  WinAAttributes
    uint8_t win_b_attributes;    // 0x03  WinBAttributes
    uint16_t win_granularity;    // 0x04  WinGranularity (КБ)
    uint16_t win_size;           // 0x06  WinSize (КБ)
    uint16_t win_a_segment;      // 0x08  WinASegment
    uint16_t win_b_segment;      // 0x0A  WinBSegment
    uint32_t win_func_ptr;       // 0x0C  WinFuncPtr
    uint16_t bytes_per_scanline; // 0x10  BytesPerScanLine  <-- это pitch!

    // VBE 1.2+
    uint16_t x_resolution;         // 0x12  XResolution
    uint16_t y_resolution;         // 0x14  YResolution
    uint8_t x_char_size;           // 0x16  XCharSize
    uint8_t y_char_size;           // 0x17  YCharSize
    uint8_t number_of_planes;      // 0x18  NumberOfPlanes
    uint8_t bits_per_pixel;        // 0x19  BitsPerPixel
    uint8_t number_of_banks;       // 0x1A  NumberOfBanks
    uint8_t memory_model;          // 0x1B  MemoryModel (6 = DirectColor)
    uint8_t bank_size;             // 0x1C  BankSize (КБ)
    uint8_t number_of_image_pages; // 0x1D  NumberOfImagePages
    uint8_t reserved1;             // 0x1E  Reserved (было 1 в VBE 1.x)

    // Direct Color поля
    uint8_t red_mask_size;          // 0x1F  RedMaskSize
    uint8_t red_field_position;     // 0x20  RedFieldPosition
    uint8_t green_mask_size;        // 0x21  GreenMaskSize
    uint8_t green_field_position;   // 0x22  GreenFieldPosition
    uint8_t blue_mask_size;         // 0x23  BlueMaskSize
    uint8_t blue_field_position;    // 0x24  BlueFieldPosition
    uint8_t rsvd_mask_size;         // 0x25  RsvdMaskSize
    uint8_t rsvd_field_position;    // 0x26  RsvdFieldPosition
    uint8_t direct_color_mode_info; // 0x27  DirectColorModeInfo

    // VBE 2.0+
    uint32_t phys_base_ptr; // 0x28  PhysBasePtr (LFB физический адрес)
    uint32_t reserved2;     // 0x2C  Reserved
    uint16_t reserved3;     // 0x30  Reserved

    // VBE 3.0+
    uint16_t linear_bytes_per_scanline;   // 0x32  LinBytesPerScanLine
    uint8_t bank_number_of_image_pages;   // 0x34  BnkNumberOfImagePages
    uint8_t linear_number_of_image_pages; // 0x35 LinNumberOfImagePages
    uint8_t linear_red_mask_size;         // 0x36  LinRedMaskSize
    uint8_t linear_red_field_position;    // 0x37  LinRedFieldPosition
    uint8_t linear_green_mask_size;       // 0x38  LinGreenMaskSize
    uint8_t linear_green_field_position;  // 0x39  LinGreenFieldPosition
    uint8_t linear_blue_mask_size;        // 0x3A  LinBlueMaskSize
    uint8_t linear_blue_field_position;   // 0x3B  LinBlueFieldPosition
    uint8_t linear_rsvd_mask_size;        // 0x3C  LinRsvdMaskSize
    uint8_t linear_rsvd_field_position;   // 0x3D  LinRsvdFieldPosition
    uint32_t max_pixel_clock;             // 0x3E  MaxPixelClock

    uint8_t reserved4[189]; // 0x42  Reserved
} __attribute__((packed)) vbe_mode_info_t;

typedef struct
{
    uint32_t lfb_phys; // физический адрес lfb (linear frame buffer)
    uint32_t lfb_virt; // виртуальный адрес
    uint32_t width;    // ширина в пикселях
    uint32_t height;   // высота в пикселях
    uint32_t pitch;    // байт на строку lfb
    uint8_t bpp;
    uint8_t red_pos;       // смещение (в битах) красного канала в 32 битном пикселе
    uint8_t red_size;      // размер (в битах) красного канала в 32 битном пикселе
    uint8_t green_pos;     // смещение (в битах) зеленого канала в 32 битном пикселе
    uint8_t green_size;    // размер (в битах) зеленого канала в 32 битном пикселе
    uint8_t blue_pos;      // смещение (в битах) синего канала в 32 битном пикселе
    uint8_t blue_size;     // размер (в битах) синего канала в 32 битном пикселе
    uint32_t size_bytes;   // размер всей видеопамяти
    uint32_t *back_buffer; // буффер видеопамяти (сначала пишем сюда потом сбрасываем на видеокарту)
} framebuffer_t;

extern framebuffer_t framebuffer;

extern bool_t framebuffer_is_ready;

void framebuffer_read_boot_info(void);
void framebuffer_init(void);

void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color);

void framebuffer_flush(void); // сброс буффера на видеокарту

#endif