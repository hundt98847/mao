OS=Linux
TARGET=x86_64-linux
LOCALEDIR=/usr/share/locale
BINUTILSRC=../binutils-2.19
BINUTILOBJ=$(BINUTILSRC)-obj-$(TARGET)
PYTHON=../python-2.5.4
PYTHONLIB=$(PYTHON)/usr/local/lib/python2.5/config/libpython2.5.a
PYTHONINCL=/usr/local/include/python2.5

ifeq ($(OS),Linux)
 PYTHONLDOPTS=-Xlinker -export-dynamic 
else
 PYTHONLDOPTS=
endif

# disable PYTHON integration for now
PYTHONLIB=
PYTHONINCL=
PYTHONLDOPTS=

LIBZ=-lz
OBJDIR=../obj-$(TARGET)
BINDIR=../bin
CC=gcc

# Allow users to set this SRCDIR, to build outside of the source directory.
SRCDIR=.
VPATH=$(SRCDIR)

CFLAGS=						\
	-D_GNU_SOURCE				\
	-I.					\
	-I$(OBJDIR)				\
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
	-DLOCALEDIR="\"$(LOCALEDIR)\""		\
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
	expr.c					\
	symbols.c

CCSRCS=						\
	ir.cc					\
	mao.cc					\
	MaoAddAdd.cc				\
	MaoCFG.cc				\
	MaoDCE.cc				\
	MaoDefs.cc				\
	MaoDebug.cc				\
	Maoi386Size.cc				\
	MaoLongInstructionSplit.cc		\
	MaoLoopAlign.cc				\
	MaoLoop16.cc				\
	MaoLoops.cc				\
	MaoMissDisp.cc				\
	MaoNopinizer.cc				\
	MaoNopKiller.cc				\
	MaoOpcodes.cc				\
	MaoOptions.cc				\
	MaoPasses.cc				\
	MaoProfile.cc				\
	MaoRedundantTestElim.cc			\
	MaoRedundantMemMove.cc			\
	MaoBranchSeparator.cc			\
	MaoRelax.cc				\
	MaoUnit.cc				\
	MaoZee.cc				\
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
	@$(MAKEDEPEND) -MQ $@;					\
	cp $(OBJDIR)/$*.P $(OBJDIR)/$*.d;			\
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//'	\
	    -e '/^$$/ d' -e 's/$$/ :/' -e 's/^ *//'		\
            < $(OBJDIR)/$*.P >> $(OBJDIR)/$*.d;			\
	rm -f $(OBJDIR)/$*.P
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o : %.cc
	@$(MAKEDEPEND) -MQ $@;					\
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

$(OBJDIR)/GenOpcodes: stamp-obj-$(TARGET) GenOpcodes.cc MaoDefs.h MaoDefs.tbl MaoUtil.h Makefile
	$(CC) $(CFLAGS) -o $(OBJDIR)/GenOpcodes $(SRCDIR)/GenOpcodes.cc -lstdc++

$(OBJDIR)/gen-opcodes.h: $(OBJDIR)/GenOpcodes
	$(OBJDIR)/GenOpcodes -p $(OBJDIR) $(BINUTILSRC)/opcodes/i386-opc.tbl $(SRCDIR)/MaoDefs.tbl

mao-$(TARGET): $(BINDIR)/mao-$(TARGET)

$(BINDIR)/mao-$(TARGET): stamp-bin $(OBJDIR)/gen-opcodes.h $(OBJS)
	$(CC) $(CFLAGS) $(PYTHONLDOPTS) -o $(BINDIR)/mao-$(TARGET) $(OBJS) -L$(BINUTILOBJ)/libiberty -L$(BINUTILOBJ)/bfd -lbfd -liberty -lstdc++ $(LIBZ) $(PYTHONLIB) -lpthread -ldl -lutil

.PHONY : clean allclean all mao-$(TARGET)

stamp-bin:
	test -d $(BINDIR) || mkdir -p $(BINDIR)
	touch stamp-bin

stamp-obj-$(TARGET):
	test -d $(OBJDIR) || mkdir -p $(OBJDIR)
	touch stamp-obj-$(TARGET)

clean : 
	-rm -f $(OBJDIR)/*
	-rm -f $(BINDIR)/mao-$(TARGET)

allclean : 
	-rm -rf ../obj-*
	-rm -rf $(BINDIR)
	-rm -f stamp-bin
	-rm -f stamp-obj-*

-include $(DEPS)
