CXX = g++
INCLUDES_FLAGS = -I./src/include/common -I./src/include/frontend
TEMPLATE_FLAGS = -I./src/template/frontend
CXXFLAGS = -std=c++20 -O2 -g -Wall -Wextra $(INCLUDES_FLAGS) $(TEMPLATE_FLAGS)
TARGET = compiler
SRCS = $(shell find src -name "*.cpp")
OBJS = $(SRCS:.cpp=.o)
BUILTIN_LL = ./builtin/builtin.ll

.PHONY: all build run clean

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	@cat $(BUILTIN_LL) >&2
	@./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)