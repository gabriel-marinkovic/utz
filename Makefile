#!/bin/make -f

# -s    tells it to not echo the commands that are being executed
# -j16  tells it to run up to 16 jobs in parallel when it can
MAKEFLAGS += -s -j16

# Makefile reminder since I'll forget it again soon:
#
#   Variables are set at parse-time, unless $(eval) is used to set them.
#   Commands are expanded at run-time.
#
#   Special ugly syntax:
#       $@  is the target
#       $^  are the dependencies
#       $<  is the first dependency (and this is relevant because the .d files add all the headers)
#       %   is a wildcard and can be used for matching
#
#   This makefile does its best to avoid any implicit rules.

start_time := $(shell date +%s%N)
all: run_tree/test
	$(if $(compiled) \
	,   echo "Compilation took $(shell expr "(" $(shell date +%s%N) - $(start_time) ")" / 1000000) ms" \
	,   echo "Nothing to be done")

c_compiler    := gcc
cpp_compiler  := g++


# Building fun objects
compile_flags := -MD -g -O1 -mrdrnd -maes -pthread
c_flags       := $(compile_flags) -std=c89
cpp_flags     := $(compile_flags) -std=c++20
link_flags    := 

sources := $(wildcard src/*.cpp) 		   \
           $(wildcard src/libraries/*.cpp) \
		   $(wildcard src/libraries/*.c)

objects := $(addprefix obj/, $(addsuffix .o, $(sources)))

obj/%.cpp.o: %.cpp
	echo "[$(cpp_compiler)] $<"
	mkdir -p $(dir $@)
	$(cpp_compiler) $(cpp_flags) -c $< -o $@

obj/%.c.o: %.c
	echo "[$(c_compiler)] $<"
	mkdir -p $(dir $@)
	$(c_compiler) $(c_flags) -c $< -o $@



run_tree/test: $(objects)
	echo "[$(cpp_compiler)] $@"
	mkdir -p $(dir $@)
	$(cpp_compiler) $^ -o $@ $(link_flags)
	$(eval compiled := true)


clean:
	rm -rf obj/
	rm -f  run_tree/test
	echo "Removed all binaries"

.PHONY: all clean

-include $(objects:.o=.d)