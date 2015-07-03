OUTPUT = build

OBJS = $(addprefix $(OUTPUT)/, \
    src/log.o src/machine/nsf.o \
    src/memguard.o \
    src/nsfinfo.o \
    src/sndhrdw/fds_snd.o \
    src/sndhrdw/fmopl.o \
    src/sndhrdw/mmc5_snd.o \
    src/sndhrdw/nes_apu.o \
    src/sndhrdw/vrc7_snd.o \
    src/sndhrdw/vrcvisnd.o \
    src/cpu/nes6502/dis6502.o \
    src/cpu/nes6502/nes6502.o)

# Brr... software archeology..
CFLAGS = -Wno-attributes -Wno-implicit-function-declaration -Wno-pointer-to-int-cast

CPPFLAGS = -DNSF_PLAYER \
           -DNES6502_MEM_ACCESS_CTRL \
           -Isrc \
           -Isrc/machine \
           -Isrc/sndhrdw \
           -Isrc/cpu/nes6502

LDFLAGS = -lm

all: nsfinfo

$(OUTPUT)/%.o : %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJS): | $(OUTPUT)

$(OUTPUT):
	mkdir -p $(OUTPUT)/src $(OUTPUT)/src/machine $(OUTPUT)/src/sndhrdw $(OUTPUT)/src/cpu/nes6502

nsfinfo: $(OBJS)
	$(LINK.o) $^ -o $(OUTPUT)/$@

clean:
	rm -rf $(OUTPUT)
