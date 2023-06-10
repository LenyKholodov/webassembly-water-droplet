OUT_DIR := dist
TARGET := $(OUT_DIR)/index.js
LINK_FLAGS := -s WASM=1 -s USE_SDL=2 -s USE_GLFW=3 -s 'EXPORTED_RUNTIME_METHODS=["UTF8ToString"]'
INCLUDE_DIRS := include
CC_FLAGS := -std=c++11 -O3 ${INCLUDE_DIRS:%=-I%}
TMP_DIR := tmp
SRCS := $(wildcard src/**/*.cpp) $(wildcard src/**/*.c) $(wildcard src/*.cpp) $(wildcard src/*.c)
OBJS := $(patsubst %.cpp,$(TMP_DIR)/%.o,$(SRCS))

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	@echo Linking $(notdir $@)...
	@emcc $(LINK_FLAGS) $(OBJS) -o $@

$(TMP_DIR)/%.o: %.cpp
	@echo Compiling $(notdir $<)...
	@mkdir -p $(dir $@)
	@emcc $(CC_FLAGS) -c $< -o $@

clean:
	@echo Cleaning...
	@rm -rf $(TMP_DIR) $(TARGET)

.PHONY: all build clean