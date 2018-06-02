TARGET     = lux-client
BUILD_DIR  = build
SRC_DIR    = src
LUX_SHARED = deps/lux-shared

DEBUG_FLAGS     = -g -O0 -ftrapv
WARNINGS_FLAGS  = \
	-Wall \
	-Wextra \
	-Wwrite-strings \
	-Winit-self \
	-Wcast-align \
	-Wcast-qual \
	-Wold-style-cast \
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

CXX       = g++
CXXFLAGS += -I$(SRC_DIR) -I$(LUX_SHARED)/src $(WARNINGS) $(DEBUG_FLAGS) -std=c++17 -pedantic
LDLIBS   += -lenet -pthread -lluajit -Wl,--whole-archive $(LUX_SHARED)/liblux.a -Wl,--no-whole-archive
LDFLAGS  +=

CPP_FILES = $(shell find $(SRC_DIR) -type f -name "*.cpp" -printf '%p ')
DEP_FILES = $(subst $(SRC_DIR),$(BUILD_DIR),$(patsubst %.cpp,%.d,$(CPP_FILES)))
OBJ_FILES = $(subst $(SRC_DIR),$(BUILD_DIR),$(patsubst %.cpp,%.o,$(CPP_FILES)))

.PHONY : clean

$(TARGET) : $(OBJ_FILES) liblux.a
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

liblux.a : $(LUX_SHARED)
	@echo "Building lux-shared..."
	@make -C $(LUX_SHARED)

clean :
	@echo "Cleaning up..."
	@$(RM) -r $(TARGET) $(BUILD_DIR)
	@make -C $(LUX_SHARED) clean

-include $(DEP_FILES)
