TARGET=x86_64-linux
DLOCALEDIR="\"/usr/share/locale\""
BINUTILOBJ=../binutils-2.19-obj-$(TARGET)
BINUTILSRC=../binutils-2.19
OBJDIR=../obj-$(TARGET)
BINDIR=../bin
CC=gcc

CFLAGS=						\
	-D_GNU_SOURCE				\
	-I.					\
	-I$(BINUTILOBJ)/gas 			\
	-I$(BINUTILSRC)/gas 			\
	-I$(BINUTILOBJ)/bfd			\
	-I$(BINUTILSRC)/bfd			\
	-I$(BINUTILOBJ)/gas/config		\
	-I$(BINUTILSRC)/gas/config		\
	-I$(BINUTILOBJ)				\
	-I$(BINUTILSRC)				\
	-I$(BINUTILSRC)/include			\
	-I$(BINUTILOBJ)/include			\
	-DLOCALEDIR=$(DLOCALEDIR)		\
	-g -O0 -Wall -fno-exceptions

MAKEDEPEND=gcc -MM $(CFLAGS) -o $(patsubst %.o,%.P,$@) $<

GAS_SRC=					\
	$(BINUTILSRC)/gas/app.c			\
	$(BINUTILSRC)/gas/atof-generic.c	\
	$(BINUTILSRC)/gas/cond.c		\
	$(BINUTILSRC)/gas/depend.c		\
	$(BINUTILSRC)/gas/dw2gencfi.c		\
	$(BINUTILSRC)/gas/ecoff.c		\
	$(BINUTILSRC)/gas/ehopt.c		\
	$(BINUTILSRC)/gas/expr.c		\
	$(BINUTILSRC)/gas/flonum-copy.c		\
	$(BINUTILSRC)/gas/flonum-konst.c	\
	$(BINUTILSRC)/gas/flonum-mult.c		\
	$(BINUTILSRC)/gas/frags.c		\
	$(BINUTILSRC)/gas/listing.c		\
	$(BINUTILSRC)/gas/hash.c		\
	$(BINUTILSRC)/gas/input-file.c		\
	$(BINUTILSRC)/gas/input-scrub.c		\
	$(BINUTILSRC)/gas/messages.c		\
	$(BINUTILSRC)/gas/macro.c		\
	$(BINUTILSRC)/gas/output-file.c		\
	$(BINUTILSRC)/gas/remap.c		\
	$(BINUTILSRC)/gas/sb.c			\
	$(BINUTILSRC)/gas/stabs.c		\
	$(BINUTILSRC)/gas/subsegs.c		\
	$(BINUTILSRC)/gas/write.c

GAS_CONFIG_SRC=					\
	$(BINUTILSRC)/gas/config/atof-ieee.c

OPCODES_SRC=					\
	$(BINUTILSRC)/opcodes/i386-opc.c

CSRCS=						\
	as.c					\
	read.c					\
	tc-i386.c				\
	dwarf2dbg.c				\
	obj-elf.c				\
	symbols.c

CCSRCS=						\
	ir.cc					\
	mao.cc					\
	MaoCFG.cc				\
	MaoDebug.cc				\
	MaoLoops.cc				\
	MaoOptions.cc				\
	MaoPasses.cc				\
	MaoUnit.cc				\
	MaoOpcodes.cc				\
	SymbolTable.cc


GAS_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(GAS_SRC)))
GAS_CONFIG_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(GAS_CONFIG_SRC)))
OPCODES_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(OPCODES_SRC)))
BINUTILS_OBJS=$(GAS_OBJS) $(GAS_CONFIG_OBJS) $(OPCODES_OBJS)

OBJS=							\
	$(BINUTILS_OBJS)				\
	$(patsubst %.c,$(OBJDIR)/%.o,$(CSRCS))		\
	$(patsubst %.cc,$(OBJDIR)/%.o,$(CCSRCS))

CDEPS=$(patsubst %.c,$(OBJDIR)/%.d,$(CSRCS))
CCDEPS=$(patsubst %.cc,$(OBJDIR)/%.d,$(CCSRCS))
DEPS=$(CDEPS) $(CCDEPS)

all: mao-$(TARGET)

$(OBJDIR)/%.o : %.c
	@$(MAKEDEPEND);						\
	cp $(OBJDIR)/$*.P $(OBJDIR)/$*.d;			\
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//'	\
	    -e '/^$$/ d' -e 's/$$/ :/' -e 's/^ *//'		\
            < $(OBJDIR)/$*.P >> $(OBJDIR)/$*.d;			\
	rm -f $(OBJDIR)/$*.P
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o : %.cc
	@$(MAKEDEPEND);						\
	cp $(OBJDIR)/$*.P $(OBJDIR)/$*.d;			\
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//'	\
	    -e '/^$$/ d' -e 's/$$/ :/' -e 's/^ *//'		\
            < $(OBJDIR)/$*.P >> $(OBJDIR)/$*.d;			\
	rm -f $(OBJDIR)/$*.P
	$(CC) $(CFLAGS) -c -o $@ $<

$(GAS_OBJS) : $(OBJDIR)/%.o : $(BINUTILSRC)/gas/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(GAS_CONFIG_OBJS) : $(OBJDIR)/%.o : $(BINUTILSRC)/gas/config/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OPCODES_OBJS) : $(OBJDIR)/%.o : $(BINUTILSRC)/opcodes/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/GenOpcodes: $(OBJDIR) GenOpcodes.cc Makefile
	$(CXX) -o $(OBJDIR)/GenOpcodes GenOpcodes.cc

gen-opcodes.h: $(OBJDIR)/GenOpcodes
	$(OBJDIR)/GenOpcodes $(BINUTILSRC)/opcodes/i386-opc.tbl

mao-$(TARGET): $(BINDIR)/mao-$(TARGET)

$(BINDIR)/mao-$(TARGET): stamp-bin gen-opcodes.h $(OBJDIR) $(OBJS)
	$(CC) -o $(BINDIR)/mao-$(TARGET) $(OBJS) -L$(BINUTILOBJ)/libiberty -L$(BINUTILOBJ)/bfd -lbfd -liberty -lstdc++ -lz

.PHONY : clean all mao-$(TARGET)

$(OBJDIR) :
	mkdir -p $(OBJDIR)

stamp-bin:
	test -d $(BINDIR) || mkdir -p $(BINDIR)
	touch stamp-bin

clean : 
	rm -f $(OBJDIR)/*
	rm -f $(BINDIR)/mao-$(TARGET)
	rm -f gen-opcodes-table.h
	rm -f gen-opcodes.h
	rm -f stamp-bin

-include $(DEPS)
