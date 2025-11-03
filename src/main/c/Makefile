CC_LINUX = gcc
CC_WINDOWS = x86_64-w64-mingw32-gcc

SRC = src/CJYaml.c
OUT_DIR = out

CFLAGS = -O2 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-overflow \
         -Wcast-align -Wpointer-arith -Wundef -Wunreachable-code \
         -fstack-protector-strong -fPIC

all: $(OUT_DIR)/cjyaml.so $(OUT_DIR)/cjyaml.dll

$(OUT_DIR)/cjyaml.so: $(SRC)
	@mkdir -p $(OUT_DIR)
	$(CC_LINUX) $(CFLAGS) -shared $< -o $@

$(OUT_DIR)/cjyaml.dll: $(SRC)
	@mkdir -p $(OUT_DIR)
	$(CC_WINDOWS) $(CFLAGS) -shared $< -o $@

clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean
