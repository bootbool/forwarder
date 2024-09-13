CC       = gcc
LD       = ld
LDFLAGS  = $(LFLAGS) -static

BUILDDIR = build
SRCDIR = $(shell pwd)
BINNAME  = daemon 
PARENT = ../
CFLAGS   = -g -O0 -pipe -Iinclude -Ilwip/linux/include -Ilwip/include -DLWIP_DEBUG -static -I$(PARENT)/include

PROGRAM  = $(SRCDIR)/$(BINNAME)

rwildcard= $(foreach d,$(wildcard $1*), \
           $(call rwildcard,$d/,$2) \
           $(filter $(subst *,%,$2),$d))

SRC      = $(call rwildcard,$(SRCDIR)/,*.c)
OBJ      = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC))

.PHONY: all clean

all : $(PROGRAM)

$(PROGRAM) : $(OBJ)
	echo "Start to compile daemon binary..."
	mkdir -p $(dir $@)
	$(CC) -g -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o : $(SRCDIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	$(RM) -rf $(BINDIR) $(BUILDDIR) $(PROGRAM)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPEND)
endif
