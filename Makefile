MAKEFLAGS += --jobs=$(shell nproc)

ifdef OS
    # Windows
    o := .obj
    x := .exe
    find := C:\Users\jtpj\w64devkit\bin\find

    cc := cl.exe

    cflags += /Zc:preprocessor
    cflags += /Z7
    cflags += /Fo: $@

    #lflags += C:\Users\jtpj\glfw\lib-vc2022\glfw3_mt.lib
    #lflags += opengl32.lib user32.lib gdi32.lib shell32.lib
    #lflags += ws2_32.lib # Link with winsock2.h.
    lflags += /Z7
    lflags += /Fe: $@
else
    # Linux
    o := .o
    x :=
    find := find

    cc := gcc
    #fsan := -fsanitize=address,undefined

    cflags += -Wall -Werror
    cflags += -Wno-unused
    cflags += -std=c99 # The lack of -pedantic allows anonymous unions in structs.
    cflags += -g3
    cflags += -MMD -MP
    cflags += -MT bin/$*.o -MT bin/$*.obj # Yucky! We tell the compiler on Linux to output dependency-tracking info for both Linux and Windows.
    cflags += -o $@

    #lflags += -L /usr/local/lib/glfw/build-x11/src/
    #lflags += -lglfw3 -lrt -lm -ldl -lX11 -lm -pthread -lffi -lGL
    lflags += -o $@

    # Run targets:
    all:  ;  bin/main$x
endif

# Build targets:
all:  bin/main$x
all:  tags

#|Temporary:
cflags += -DDEBUG
cflags += -DDEBUG_MEMORY_CONTEXT
cflags += $(fsan)
ifeq ($(cc),gcc)
  cflags += -Wno-missing-braces
endif

lflags += $(fsan)

sources := $(shell $(find) src -type f)
non_mains := $(shell grep -L '^int main' $(sources))
shared_obj := $(patsubst src/%.c,bin/%$o,$(filter %.c,$(non_mains)))
deps := $(patsubst src/%.c,bin/%.d,$(filter %.c,$(sources)))
src_dirs := $(dir $(sources))

$(shell mkdir -p $(patsubst src%,bin%,$(src_dirs)))


bin/%$x:  bin/%$o $(shared_obj);  $(cc) $^ $(lflags)

bin/%$o:  src/%.c;  $(cc) -c $(cflags) $<

tags:  $(sources);  ctags --recurse src/

tidy:  ;  rm -f core.*
clean:  tidy;  rm -rf bin tags

bin/%.d: ;
include $(deps)
