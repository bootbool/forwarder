
OUTPUT := .
CLANG ?= clang
THIRD_PARTY = $(abspath ./third_party/)
LIBBPF_OBJ := $(THIRD_PARTY)/libbpf/src/libbpf.a
BPFTOOL ?= $(THIRD_PARTY)/bpftool/src/bpftool
ARCH ?= x86
VMLINUX := $(THIRD_PARTY)/vmlinux/vmlinux.h

INCLUDES := -I$(OUTPUT) -I$(THIRD_PARTY)/libbpf/include/uapi -I$(dir $(VMLINUX)) -I../include
CFLAGS := -g -Wall -O0
ALL_LDFLAGS := -lelf -lz 

CLANG_BPF_SYS_INCLUDES ?= $(shell $(CLANG) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')


APPS = redirect

.PHONY: all clean
all: $(APPS)

clean:
	rm -rf $(APPS) *.o *.skel.h

# Build BPF code
$(OUTPUT)/%.bpf.o: %.bpf.c $(LIBBPF_OBJ) $(wildcard %.h) $(VMLINUX) | $(OUTPUT)
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH)		      \
		     $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES)		      \
		     -c $(filter %.c,$^) -o $(patsubst %.bpf.o,%.tmp.bpf.o,$@)
	$(BPFTOOL) gen object $@ $(patsubst %.bpf.o,%.tmp.bpf.o,$@)

# Generate BPF skeletons
$(OUTPUT)/%.skel.h : $(OUTPUT)/%.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

$(OUTPUT)/%.o : %.c $(wildcard %.h) %.skel.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(filter %.c,$^) -o $@

$(APPS): $(OUTPUT)/$(APPS).skel.h  $(OUTPUT)/$(APPS).o  $(LIBBPF_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(ALL_LDFLAGS) -o $@
