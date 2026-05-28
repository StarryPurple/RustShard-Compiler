CXX = g++
TARGET = compiler
SRCS = $(shell find src -name "*.cpp")
OBJS = $(SRCS:.cpp=.o)
BUILTIN_ASM_PATH = ./builtin/builtin.s
INCLUDES_FLAGS = -I./include
BUILTIN_FLAGS = -DBUILTIN_ASM_PATH=\"$(BUILTIN_ASM_PATH)\"
CXXFLAGS = -std=c++20 -O0 -pipe -DNDEBUG $(INCLUDES_FLAGS) $(BUILTIN_FLAGS)# -g -Wall -Wextra

.PHONY: all build run clean

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	rm -f $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: build
	@./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)