TARGET=x86_64
DLOCALEDIR="\"/usr/share/locale\""
BINUTILOBJ=../binutils-2.19-obj-$(TARGET)
BINUTILSRC=../binutils-2.19
OBJDIR=../obj-$(TARGET)
BINDIR=../bin
CC=gcc

CFLAGS=-D_GNU_SOURCE -I. -I$(BINUTILOBJ)/gas -I$(BINUTILSRC)/gas -I$(BINUTILOBJ)/bfd -I$(BINUTILSRC)/bfd -I$(BINUTILOBJ)/gas/config -I$(BINUTILSRC)/gas/config -I$(BINUTILOBJ) -I$(BINUTILSRC) -I$(BINUTILSRC)/include -I$(BINUTILOBJ)/include \
	-DLOCALEDIR=$(DLOCALEDIR) \
	-g -O0 -Wall

OBJS=$(OBJDIR)/as.o \
	$(OBJDIR)/mao.o \
	$(OBJDIR)/ir.o \
	$(OBJDIR)/MaoUnit.o \
	$(OBJDIR)/MaoOptions.o \
	$(OBJDIR)/SymbolTable.o \
	$(OBJDIR)/read.o \
	$(OBJDIR)/tc-i386.o\
	$(OBJDIR)/app.o \
	$(OBJDIR)/atof-generic.o \
	$(OBJDIR)/atof-ieee.o \
	$(OBJDIR)/cond.o \
	$(OBJDIR)/depend.o \
	$(OBJDIR)/dw2gencfi.o \
	$(OBJDIR)/dwarf2dbg.o \
	$(OBJDIR)/ecoff.o \
	$(OBJDIR)/ehopt.o \
	$(OBJDIR)/expr.o \
	$(OBJDIR)/flonum-copy.o \
	$(OBJDIR)/flonum-konst.o \
	$(OBJDIR)/flonum-mult.o \
	$(OBJDIR)/frags.o \
	$(OBJDIR)/listing.o \
	$(OBJDIR)/hash.o \
	$(OBJDIR)/i386-opc.o \
	$(OBJDIR)/input-file.o \
	$(OBJDIR)/input-scrub.o \
	$(OBJDIR)/messages.o \
	$(OBJDIR)/macro.o \
	$(OBJDIR)/obj-elf.o \
	$(OBJDIR)/output-file.o \
	$(OBJDIR)/remap.o \
	$(OBJDIR)/sb.o \
	$(OBJDIR)/stabs.o \
	$(OBJDIR)/subsegs.o \
	$(OBJDIR)/symbols.o \
	$(OBJDIR)/write.o


all: mao-$(TARGET)

mao-$(TARGET): $(BINDIR)/mao-$(TARGET)

$(BINDIR)/mao-$(TARGET): stamp-bin $(OBJDIR) $(OBJS)
	$(CC) -o $(BINDIR)/mao-$(TARGET) $(OBJS) -L $(BINUTILOBJ)/libiberty -L $(BINUTILOBJ)/bfd -lbfd -liberty -lstdc++ -lz

.PHONY : clean all mao-$(TARGET)

$(OBJDIR) :
	mkdir -p $(OBJDIR)

stamp-bin:
	test -d $(BINDIR) || mkdir -p $(BINDIR)
	touch stamp-bin

# object files

$(OBJDIR)/mao.o: mao.cc mao.h
	$(CC) $(CFLAGS) -c -o $@ mao.cc

$(OBJDIR)/ir.o: ir.cc ir.h  irlink.h
	$(CC) $(CFLAGS) -c -o $@ ir.cc

$(OBJDIR)/MaoUnit.o: MaoUnit.cc MaoUnit.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/MaoOptions.o: MaoOptions.cc MaoOptions.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/SymbolTable.o: SymbolTable.cc SymbolTable.h irlink.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/obj-elf.o: obj-elf.c
	$(CC) $(CFLAGS)  -c -o $@ $<

$(OBJDIR)/as.o: as.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/read.o: read.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/symbols.o: symbols.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/dwarf2dbg.o: dwarf2dbg.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tc-i386.o: tc-i386.c
	$(CC) $(CFLAGS) -c -o $@ tc-i386.c

$(OBJDIR)/app.o: $(BINUTILSRC)/gas/app.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/app.c

$(OBJDIR)/atof-generic.o: $(BINUTILSRC)/gas/atof-generic.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/atof-generic.c

$(OBJDIR)/atof-ieee.o: $(BINUTILSRC)/gas/config/atof-ieee.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/config/atof-ieee.c

$(OBJDIR)/cond.o: $(BINUTILSRC)/gas/cond.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/cond.c

$(OBJDIR)/depend.o: $(BINUTILSRC)/gas/depend.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/depend.c

$(OBJDIR)/dw2gencfi.o: $(BINUTILSRC)/gas/dw2gencfi.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/dw2gencfi.c

$(OBJDIR)/ecoff.o: $(BINUTILSRC)/gas/ecoff.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/ecoff.c

$(OBJDIR)/ehopt.o: $(BINUTILSRC)/gas/ehopt.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/ehopt.c

$(OBJDIR)/expr.o: $(BINUTILSRC)/gas/expr.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/expr.c

$(OBJDIR)/flonum-copy.o: $(BINUTILSRC)/gas/flonum-copy.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/flonum-copy.c

$(OBJDIR)/flonum-konst.o: $(BINUTILSRC)/gas/flonum-konst.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/flonum-konst.c

$(OBJDIR)/flonum-mult.o: $(BINUTILSRC)/gas/flonum-mult.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/flonum-mult.c

$(OBJDIR)/frags.o: $(BINUTILSRC)/gas/frags.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/frags.c

$(OBJDIR)/listing.o: $(BINUTILSRC)/gas/listing.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/listing.c

$(OBJDIR)/hash.o: $(BINUTILSRC)/gas/hash.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/hash.c

$(OBJDIR)/i386-opc.o: $(BINUTILSRC)/opcodes/i386-opc.c
	$(CC) $(CFLAGS) -I$(BINUTILSRC)/opcodes -c -o $@ $(BINUTILSRC)/opcodes/i386-opc.c

$(OBJDIR)/input-file.o: $(BINUTILSRC)/gas/input-file.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/input-file.c

$(OBJDIR)/input-scrub.o: $(BINUTILSRC)/gas/input-scrub.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/input-scrub.c

$(OBJDIR)/messages.o: $(BINUTILSRC)/gas/messages.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/messages.c

$(OBJDIR)/macro.o: $(BINUTILSRC)/gas/macro.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/macro.c

$(OBJDIR)/output-file.o: $(BINUTILSRC)/gas/output-file.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/output-file.c

$(OBJDIR)/remap.o: $(BINUTILSRC)/gas/remap.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/remap.c

$(OBJDIR)/sb.o: $(BINUTILSRC)/gas/sb.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/sb.c

$(OBJDIR)/stabs.o: $(BINUTILSRC)/gas/stabs.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/stabs.c

$(OBJDIR)/subsegs.o: $(BINUTILSRC)/gas/subsegs.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/subsegs.c

$(OBJDIR)/write.o: $(BINUTILSRC)/gas/write.c
	$(CC) $(CFLAGS) -c -o $@ $(BINUTILSRC)/gas/write.c

clean : 
	rm  $(OBJDIR)/*
	rm  $(BINDIR)/mao-$(TARGET)
