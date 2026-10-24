BUILD_DIR = $(dir $(PROG_TARGET))$(PROG_NAME)

SOURCES_C   = $(wildcard $(PROG_SRC_DIR)/*.c)
SOURCES_ASM = $(wildcard $(PROG_SRC_DIR)/*.asm)

$(info PROG_SRC_DIR = [$(PROG_SRC_DIR)])
$(info SOURCES_C    = [$(SOURCES_C)])

OBJS_C   = $(patsubst $(PROG_SRC_DIR)/%.c,   $(BUILD_DIR)/%.o, $(SOURCES_C))
OBJS_ASM = $(patsubst $(PROG_SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(SOURCES_ASM))
OBJS     = $(OBJS_C) $(OBJS_ASM)

# подключаем сгенерированные файлы зависимостей
-include $(OBJS:.o=.d)

include ../../rules.mk