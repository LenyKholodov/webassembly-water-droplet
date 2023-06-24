OUT_DIR := dist
TARGET := $(OUT_DIR)/index.js
MEDIA_FILES := $(wildcard media/textures/*) $(wildcard media/shaders/*) $(wildcard media/meshes/*)
COMMON_FLAGS += -s USE_SDL=2 -sUSE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png","jpg"]' -s USE_BULLET=1
LINK_FLAGS := -s WASM=1  -sNO_DISABLE_EXCEPTION_CATCHING  -s 'EXPORTED_RUNTIME_METHODS=["UTF8ToString"]' 
LINK_FLAGS +=  -sALLOW_MEMORY_GROWTH
LINK_FLAGS += -gsource-map --source-map-base "http://localhost:8080/"
LINK_FLAGS += $(COMMON_FLAGS)
#LINK_FLAGS += -sSTACK_SIZE=50MB -sINITIAL_MEMORY=120MB
LINK_FLAGS += $(MEDIA_FILES:%=--embed-file "%") -s USE_GLFW=3
INCLUDE_DIRS := include .
CC_FLAGS := -std=c++17 ${INCLUDE_DIRS:%=-I%}
CC_FLAGS += -O3 -Wbad-function-cast -Wcast-function-type
CC_FLAGS += $(COMMON_FLAGS)
#CC_FLAGS += -g3 --tracing #remove, only for debug info
TMP_DIR := tmp
SRCS := $(wildcard src/**/*.cpp) $(wildcard src/**/**/*.cpp) $(wildcard src/**/*.c) $(wildcard src/*.cpp) $(wildcard src/*.c)
OBJS := $(patsubst %.cpp,$(TMP_DIR)/%.o,$(SRCS))

all: build

build: $(TARGET)

$(TARGET): $(OBJS) $(MEDIA_FILES)
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