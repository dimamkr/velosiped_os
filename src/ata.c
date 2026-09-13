#include "ata.h"

void ata_parse_identify_answer(void *cmd_answer_buffer, ata_basic_identify_data_t *result)
{
    memset(result, 0, sizeof(ata_basic_identify_data_t));

    // полученный буффер, согласно спецификации, интерпретируется как массив из 256 2-байтовых слов
    uint16_t *answer_words = (uint16_t *)cmd_answer_buffer;

    // парсим строку serial
    for (int i = 0; i < 10; i++)
    {
        result->serial[i * 2] = (answer_words[10 + i] >> 8) & 0xFF;
        result->serial[i * 2 + 1] = answer_words[10 + i] & 0xFF;
    }

    // парсим строку model
    for (int i = 0; i < 20; i++)
    {
        result->model[i * 2] = (answer_words[27 + i] >> 8) & 0xFF;
        result->model[i * 2 + 1] = answer_words[27 + i] & 0xFF;
    }

    // парсим количество секторов

    // для LBA48
    result->sectors = (uint64_t)answer_words[100] |
                      ((uint64_t)answer_words[101] << 16) |
                      ((uint64_t)answer_words[102] << 32) |
                      ((uint64_t)answer_words[103] << 48);

    // для LBA28
    if (result->sectors == 0)
        result->sectors = (uint64_t)answer_words[60] | ((uint64_t)answer_words[61] << 16);

    result->lba48_supported = (answer_words[83] & (1 << 10));
    result->ncq_supported = (answer_words[76] & (1 << 8));
    result->dma_supported = (answer_words[49] & (1 << 8));
}