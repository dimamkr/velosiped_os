# общие правила сборки пользовательских программ

# цель по умолчанию собирает финальный ELF
all: $(PROG_TARGET)

# стандартное правило линковки
$(PROG_TARGET): $(OBJS)
	@echo "  [ULD]    $(PROG_NAME).elf"
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

# стандартное правило компиляции C файлов
$(BUILD_DIR)/%.o: $(PROG_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  [UCC]    $(PROG_NAME)/$(notdir $<)"
	$(CC) $(CFLAGS) $(PROG_CFLAGS) -c -o $@ $<

# стандартное правило компиляции ASM файлов
$(BUILD_DIR)/%.o: $(PROG_SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo "  [UCC]    $(PROG_NAME)/$(notdir $<)"
	$(NASM) $(NASMFLAGS) -o $@ $<