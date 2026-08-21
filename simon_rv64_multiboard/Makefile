include config.mk

BUILD := build
SCRIPTS := scripts

RAW_BIN := $(BUILD)/$(YOUR_RAW_BIN)
INTERMEDIATE_FILES = $(BUILD)/boot.elf $(BUILD)/boot.bin $(BUILD)/simon.elf $(BUILD)/simon.bin $(BUILD)/simon.hex0

all: $(RAW_BIN)

# Старое имя цели (`make image_qemu.bin`) ведёт в build/
.PHONY: $(YOUR_RAW_BIN)
$(YOUR_RAW_BIN): $(RAW_BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(RAW_BIN): $(BOOT_SRC) $(PAYLOAD_SRC) $(FILES_HEX0) $(BOARD_HEADER) config.mk boards/$(BOARD).mk | $(BUILD)
	$(CC) $(CFLAGS) $(BOARD_CFLAGS) $(TEXT_FLAG)$(DDR_ADDR) $(BOOT_SRC) -o $(BUILD)/boot.elf
	$(OBJCOPY) -O binary $(BUILD)/boot.elf $(BUILD)/boot.bin
	$(CC) $(CFLAGS) $(BOARD_CFLAGS) $(TEXT_FLAG)$(DDR_ADDR) $(PAYLOAD_SRC) -o $(BUILD)/simon.elf
	$(OBJCOPY) -O binary $(BUILD)/simon.elf $(BUILD)/simon.bin
	python3 $(SCRIPTS)/bin-to-hex0.sh $(BUILD)/simon.bin $(BUILD)/simon.hex0
	printf '\0' >> $(BUILD)/simon.hex0
	cat $(BUILD)/boot.bin $(BUILD)/simon.hex0 $(FILES_HEX0) > $@
	@echo "ГОТОВО: $@"
	@stat -c "Размер: %s bytes" $@ 2>/dev/null || stat -f "Размер: %z bytes" $@
	@echo "BOARD=$(BOARD)"
	@echo "U-Boot raw: ext4load mmc 0:5 $(DDR_ADDR) $@; go $(DDR_ADDR)"

fullflash: $(RAW_BIN)
ifeq ($(SUPPORTS_FULLFLASH),1)
	PAYLOAD=$(RAW_BIN) WRAPPED=$(BUILD)/$(WRAPPED_BIN) FLASH=$(BUILD)/$(FULLFLASH_BIN) SPL=$(UBOOT_SPL_BIN) LOAD_ADDR=$(DDR_ADDR) IMAGE_NAME=$(UBOOT_IMAGE_NAME) python3 $(SCRIPTS)/make_uboot_flash.py
	@echo "ГОТОВО FULLFLASH: $(BUILD)/$(FULLFLASH_BIN)"
else
	@echo "BOARD=$(BOARD) does not use fullflash. Use raw image: $(RAW_BIN)"
endif

qemu:
	$(MAKE) BOARD=qemu_virt clean_tmp
	$(MAKE) BOARD=qemu_virt
	qemu-system-riscv64 -M virt -smp 4 -m 6G -nographic -bios $(BUILD)/image_qemu.bin

prepare-files:
	$(SCRIPTS)/pack.sh

clean_tmp:
	rm -f $(INTERMEDIATE_FILES)

clean: clean_tmp
	rm -f $(BUILD)/image_*.bin $(BUILD)/flash_me_*.bin $(BUILD)/simon_wrapped_*.img $(BUILD)/image.bin

.PHONY: all fullflash qemu prepare-files clean clean_tmp
