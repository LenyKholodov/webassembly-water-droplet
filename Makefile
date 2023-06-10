OUT_DIR := dist
TARGET := $(OUT_DIR)/index.js
LINK_FLAGS := -s WASM=1 -s USE_SDL=2 -s USE_GLFW=3
CC_FLAGS := -std=c++11 -O3
TMP_DIR := tmp
SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst %.cpp,$(TMP_DIR)/%.o,$(SRCS))

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	@echo Linking $(notdir $@)...
	@emcc $(OBJS) $(LINK_FLAGS) -o $@

$(TMP_DIR)/%.o: %.cpp
	@echo Compiling $(notdir $<)...
	@mkdir -p $(dir $@)
	@emcc $(CC_FLAGS) -c $< -o $@

clean:
	@echo Cleaning...
	@rm -rf $(TMP_DIR) $(TARGET)

.PHONY: all build clean