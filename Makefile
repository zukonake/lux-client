TARGET     = lux-client
BUILD_DIR  = build
SRC_DIR    = src

DEBUG_FLAGS     = -g -O0 -ftrapv
WARNINGS_FLAGS  = \
	-Wall \
	-Wextra \
	-Werror \
	-Wwrite-strings \
	-Winit-self \
	-Wcast-align \
	-Wcast-qual \
	-Wno-old-style-cast \
	-Wpointer-arith \
	-Wstrict-aliasing \
	-Wformat=2 \
	-Wuninitialized \
	-Wmissing-declarations \
	-Woverloaded-virtual \
	-Wnon-virtual-dtor \
	-Wctor-dtor-privacy \
	-Wno-long-long \
	-Weffc++ \
	-Wconversion

CXX       = clang++
CXXFLAGS += -I$(SRC_DIR) -Ideps/lux-shared/src -isystem include \
	    $(WARNINGS_FLAGS) $(DEBUG_FLAGS) -std=c++17 -pedantic
LDLIBS   += -lenet -pthread -llux -llodepng -lGL -lglfw -lX11 -lXrandr -lXi -ldl
LDFLAGS  += -Ldeps/lux-shared -Ldeps/lodepng

CPP_FILES = $(shell find $(SRC_DIR) -type f -name "*.cpp" -printf '%p ')
DEP_FILES = $(subst $(SRC_DIR),$(BUILD_DIR),$(patsubst %.cpp,%.d,$(CPP_FILES)))
OBJ_FILES = $(subst $(SRC_DIR),$(BUILD_DIR),$(patsubst %.cpp,%.o,$(CPP_FILES)))

.PHONY : clean

$(TARGET) : $(OBJ_FILES) deps/lux-shared/liblux.a deps/lodepng/liblodepng.a
	@echo "Linking $@..."
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $(OBJ_FILES) -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.cpp $(BUILD_DIR)/%.d
	@echo "Building $@..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.d : $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -MM $(CXXFLAGS) $< > $@
	@sed -i "1s~^~$(dir $@)~" $@

deps/lux-shared/liblux.a : deps/lux-shared
	@echo "Building lux-shared..."
	@make -C deps/lux-shared

deps/lodepng/liblodepng.a : deps/lodepng
	@echo "Building lodepng..."
	@make -C deps/lodepng

clean :
	@echo "Cleaning up..."
	@$(RM) -r $(TARGET) $(BUILD_DIR)
	@make -C deps/lux-shared clean
	@make -C deps/lodepng clean

-include $(DEP_FILES)
