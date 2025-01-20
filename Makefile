MAKEFLAGS += --jobs=$(shell nproc)

ifdef OS
    #
    # Windows
    #
    o := .obj
    x := .exe
    find := C:\Users\jtpj\w64devkit\bin\find

    cc := cl.exe

    cflags += -Zc:preprocessor
    cflags += -Z7
    cflags += -Fo: $@
    cflags += -IC:\Users\jtpj\vcpkg-master\installed\x64-windows-static\include # For pthreads

    lflags += C:\Users\jtpj\vcpkg-master\installed\x64-windows-static\lib\pthreadVC3.lib # For pthreads
    lflags += -Z7
    lflags += -Fe: $@
else
    #
    # Linux
    #
    o := .o
    x :=
    find := find

    cc := gcc
    #cc := clang # Also works

    cflags += -Wall -Werror
    cflags += -Wno-unused
    cflags += -std=c99
    cflags += -g3
    cflags += -O2
    #cflags += -DNDEBUG
    cflags += -MMD -MP
    cflags += -MT bin/$*.o -MT bin/$*.obj # Yucky! We tell the compiler on Linux to output dependency-tracking files for both Linux and Windows.
    cflags += -o $@

    lflags += -pthread
    lflags += -o $@
endif

ifeq ($(cc),gcc)
  cflags += -Wno-missing-braces
endif

sources    := $(shell $(find) src -type f)
non_mains  := $(shell grep -L '^int main' $(sources))
shared_obj := $(patsubst src/%.c,bin/%$o,$(filter %.c,$(non_mains)))
deps       := $(patsubst src/%.c,bin/%.d,$(filter %.c,$(sources)))
src_dirs   := $(dir $(sources))
exes       := $(patsubst src/%.c,bin/%$x,$(filter-out $(non_mains),$(sources)))

$(shell mkdir -p $(patsubst src%,bin%,$(src_dirs)))

# Build targets:
all:  $(exes)
all:  tags

# Run targets:
#all:  ;  bin/test$x

bin/%$x:  bin/%$o $(shared_obj);  $(cc) $^ $(lflags)

bin/%$o:  src/%.c;  $(cc) -c $(cflags) $<

tags:  $(sources);  ctags --recurse src

tidy:  ;  rm -f core.*
clean:  tidy;  rm -rf bin tags

bin/%.d: ;
include $(deps)
