OUT_DIR := dist
TARGET := $(OUT_DIR)/index.js
MEDIA_FILES := $(wildcard media/textures/*) $(wildcard media/shaders/*)
LINK_FLAGS := -s WASM=1 -s USE_SDL=2 -s USE_GLFW=3 -sNO_DISABLE_EXCEPTION_CATCHING  -s 'EXPORTED_RUNTIME_METHODS=["UTF8ToString"]' -sUSE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png","jpg"]'
LINK_FLAGS += $(MEDIA_FILES:%=--embed-file "%")
INCLUDE_DIRS := include
CC_FLAGS := -std=c++17 -O3 ${INCLUDE_DIRS:%=-I%}
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