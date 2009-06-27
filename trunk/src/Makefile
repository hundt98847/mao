OS=Linux
TARGET=x86_64-linux
DEVPREFIX=
LOCALEDIR=/usr/share/locale
BINUTILSRC=../binutils-2.19
BINUTILOBJ=$(BINUTILSRC)-obj-$(TARGET)
PYTHON=../python-2.5.4
PYTHONLIB=$(PYTHON)/usr/local/lib/python2.5/config/libpython2.5.a
PYTHONINCL=/usr/local/include/python2.5
REVDEF := -D'REV_NUM="$(shell svnversion -n .)"'

ifeq ($(OS),Linux)
 PYTHONLDOPTS=-Xlinker -export-dynamic 
 DYNFLAGS=-fPIC -shared
 DYNLIBEXT=so
else
 PYTHONLDOPTS=
 DYNFLAGS=-fPIC -dynamiclib -undefined dynamic_lookup
 DYNLIBEXT=dylib
endif

# disable PYTHON integration for now
PYTHONLIB=
PYTHONINCL=
PYTHONLDOPTS=

LIBZ=-lz
OBJDIR=../obj-$(TARGET)
BINDIR=../bin
HEADERDIR=../include
CC=gcc

# Allow users to set this SRCDIR, to build outside of the source directory.
SRCDIR=.
VPATH=$(SRCDIR)

CFLAGS=						\
	 $(REVDEF)                              \
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
	dw2gencfi.c				\
	obj-elf.c				\
	expr.c					\
	symbols.c

CCSRCS=						\
	ir.cc					\
	mao.cc					\
	MaoAddAdd.cc				\
	MaoBackBranchAlign.cc			\
	MaoCFG.cc				\
	MaoDCE.cc				\
	MaoDefs.cc				\
	MaoDebug.cc				\
	Maoi386Size.cc				\
	MaoLoopAlign.cc				\
	MaoLoop16.cc				\
	MaoLoops.cc				\
	MaoMissDisp.cc				\
	MaoNopinizer.cc				\
	MaoNopKiller.cc				\
	MaoOpcodes.cc				\
	MaoOptions.cc				\
	MaoPasses.cc				\
	MaoPlugin.cc				\
	MaoPrefetchNta.cc			\
	MaoProfile.cc				\
	MaoRedundantTestElim.cc			\
	MaoRedundantMemMove.cc			\
	MaoBranchSeparator.cc			\
	MaoScheduler.cc				\
	MaoRatFinder.cc				\
	MaoRelax.cc				\
	MaoUnit.cc				\
	MaoZee.cc				\
	SymbolTable.cc

PLUGIN_CCSRCS=					\
	MaoTestPlugin.cc

PLUGINS=MaoTestPlugin

GAS_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(GAS_SRC)))
GAS_CONFIG_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(GAS_CONFIG_SRC)))
OPCODES_OBJS=$(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(OPCODES_SRC)))
BINUTILS_OBJS=$(GAS_OBJS) $(GAS_CONFIG_OBJS) $(OPCODES_OBJS)
PLUGIN_OBJS=$(patsubst %.cc,$(OBJDIR)/%.o,$(PLUGIN_CCSRCS))
PLUGIN_TARGETS=$(patsubst %,$(BINDIR)/%-$(TARGET).$(DYNLIBEXT),$(PLUGINS))

OBJS=							\
	$(BINUTILS_OBJS)				\
	$(patsubst %.c,$(OBJDIR)/%.o,$(CSRCS))		\
	$(patsubst %.cc,$(OBJDIR)/%.o,$(CCSRCS))

CDEPS=$(patsubst %.c,$(OBJDIR)/%.d,$(CSRCS))
CCDEPS=$(patsubst %.cc,$(OBJDIR)/%.d,$(CCSRCS))
PLUGINDEPS=$(patsubst %.cc,$(OBJDIR)/%.d,$(PLUGIN_CCSRCS))
DEPS=$(CDEPS) $(CCDEPS) $(PLUGINDEPS)

all: mao-$(DEVPREFIX)$(TARGET) $(PLUGIN_TARGETS)

# C source rule
$(OBJDIR)/%.o : %.c stamp-obj-$(TARGET) $(OBJDIR)/gen-opcodes.h
	@$(MAKEDEPEND) -MQ $@;					\
	cp $(OBJDIR)/$*.P $(OBJDIR)/$*.d;			\
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//'	\
	    -e '/^$$/ d' -e 's/$$/ :/' -e 's/^ *//'		\
            < $(OBJDIR)/$*.P >> $(OBJDIR)/$*.d;			\
	rm -f $(OBJDIR)/$*.P
	$(CC) $(CFLAGS) -c -o $@ $<

# C++ source rule
$(OBJDIR)/%.o : %.cc stamp-obj-$(TARGET) $(OBJDIR)/gen-opcodes.h
	@$(MAKEDEPEND) -MQ $@;					\
	cp $(OBJDIR)/$*.P $(OBJDIR)/$*.d;			\
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//'	\
	    -e '/^$$/ d' -e 's/$$/ :/' -e 's/^ *//'		\
            < $(OBJDIR)/$*.P >> $(OBJDIR)/$*.d;			\
	rm -f $(OBJDIR)/$*.P
	$(CC) $(CFLAGS) -c -o $@ $<

# Plugin source rule
$(PLUGIN_OBJS) : $(OBJDIR)/%.o : %.cc stamp-obj-$(TARGET) $(OBJDIR)/gen-opcodes.h
	@$(MAKEDEPEND) -MQ $@;					\
	cp $(OBJDIR)/$*.P $(OBJDIR)/$*.d;			\
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//'	\
	    -e '/^$$/ d' -e 's/$$/ :/' -e 's/^ *//'		\
            < $(OBJDIR)/$*.P >> $(OBJDIR)/$*.d;			\
	rm -f $(OBJDIR)/$*.P
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

$(GAS_OBJS) : $(OBJDIR)/%.o : $(BINUTILSRC)/gas/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(GAS_CONFIG_OBJS) : $(OBJDIR)/%.o : $(BINUTILSRC)/gas/config/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OPCODES_OBJS) : $(OBJDIR)/%.o : $(BINUTILSRC)/opcodes/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/GenOpcodes: stamp-obj-$(TARGET) GenOpcodes.cc MaoDefs.h MaoDefs.tbl MaoUses.tbl MaoUtil.h Makefile
	$(CC) $(CFLAGS) -o $(OBJDIR)/GenOpcodes $(SRCDIR)/GenOpcodes.cc -lstdc++

$(OBJDIR)/gen-opcodes.h: $(OBJDIR)/GenOpcodes
	$(OBJDIR)/GenOpcodes -p $(OBJDIR) $(BINUTILSRC)/opcodes/i386-opc.tbl $(SRCDIR)/MaoDefs.tbl $(SRCDIR)/MaoUses.tbl

mao-$(DEVPREFIX)$(TARGET): $(BINDIR)/mao-$(DEVPREFIX)$(TARGET)

$(BINDIR)/mao-$(DEVPREFIX)$(TARGET): stamp-bin $(OBJDIR)/gen-opcodes.h $(OBJS)
	$(CC) $(CFLAGS) $(PYTHONLDOPTS) -rdynamic -o $(BINDIR)/mao-$(DEVPREFIX)$(TARGET) $(OBJS) -L$(BINUTILOBJ)/libiberty -L$(BINUTILOBJ)/bfd -lbfd -liberty -lstdc++ $(LIBZ) $(PYTHONLIB) -lpthread -ldl -lutil

mao: all
	ln -s $(BINDIR)/mao-$(DEVPREFIX)$(TARGET) $(BINDIR)/mao

$(PLUGIN_TARGETS) : $(BINDIR)/%-$(TARGET).$(DYNLIBEXT) : $(OBJDIR)/%.o stamp-bin
	$(CC) $(CFLAGS) $(DYNFLAGS) -o $@ $<

.PHONY : clean allclean all mao-$(DEVPREFIX)$(TARGET) headers mao


MAO_HEADERS = $(SRCDIR)/MaoCFG.h $(SRCDIR)/MaoDebug.h $(SRCDIR)/MaoDefs.h \
	      $(SRCDIR)/MaoLoops.h $(SRCDIR)/MaoOptions.h $(SRCDIR)/MaoPasses.h\
	      $(SRCDIR)/MaoPlugin.h $(SRCDIR)/MaoStats.h $(SRCDIR)/MaoUnit.h \
	      $(SRCDIR)/MaoUtil.h $(SRCDIR)/SymbolTable.h \
	      $(OBJDIR)/gen-opcodes.h $(SRCDIR)/mao.h $(SRCDIR)/irlink.h \
              $(SRCDIR)/expr.h


MAO_BINUTILS_HEADERS = $(BINUTILSRC)/gas/config/tc-i386.h \
	    	       $(BINUTILSRC)/gas/config/te-linux.h \
	               $(BINUTILSRC)/gas/config/obj-elf.h \
	               $(BINUTILSRC)/gas/read.h \
	               $(BINUTILSRC)/gas/symbols.h \
	               $(BINUTILSRC)/gas/listing.h \
	               $(BINUTILSRC)/gas/obj.h \
	               $(BINUTILSRC)/gas/asintl.h \
	               $(BINUTILSRC)/gas/flonum.h \
	               $(BINUTILSRC)/gas/bit_fix.h \
	               $(BINUTILSRC)/gas/as.h \
	               $(BINUTILSRC)/gas/dwarf2dbg.h \
	               $(BINUTILSRC)/gas/bignum.h \
	               $(BINUTILSRC)/gas/hash.h \
	               $(BINUTILSRC)/gas/tc.h \
	               $(BINUTILSRC)/gas/frags.h \
	               $(BINUTILSRC)/gas/write.h \
	               $(BINUTILOBJ)/gas/obj-format.h \
	               $(BINUTILOBJ)/gas/config.h \
	               $(BINUTILOBJ)/gas/targ-cpu.h \
	               $(BINUTILOBJ)/gas/targ-env.h \
	               $(BINUTILSRC)/include/ansidecl.h \
	               $(BINUTILSRC)/include/symcat.h \
	               $(BINUTILSRC)/include/progress.h \
	               $(BINUTILSRC)/include/bfdlink.h \
	               $(BINUTILSRC)/include/fopen-same.h \
	               $(BINUTILSRC)/include/libiberty.h \
	               $(BINUTILOBJ)/bfd/bfd.h

MAO_ELF_HEADERS = $(BINUTILSRC)/include/elf/common.h \
	          $(BINUTILSRC)/include/elf/internal.h \
	          $(BINUTILSRC)/include/elf/external.h

MAO_BFD_HEADERS = $(BINUTILSRC)/bfd/elf-bfd.h

MAO_INCLUDE_OPCODE_HEADERS = $(BINUTILSRC)/include/opcode/i386.h

MAO_OPCODES_HEADERS = $(BINUTILSRC)/opcodes/i386-opc.h

headers:
	mkdir -p $(HEADERDIR)
	cp $(MAO_HEADERS) $(HEADERDIR)
	mkdir -p $(HEADERDIR)/binutils
	cp $(MAO_BINUTILS_HEADERS) $(HEADERDIR)/binutils
	mkdir -p $(HEADERDIR)/binutils/bfd
	cp $(MAO_BFD_HEADERS) $(HEADERDIR)/binutils/bfd
	mkdir -p $(HEADERDIR)/binutils/elf
	cp $(MAO_ELF_HEADERS) $(HEADERDIR)/binutils/elf
	mkdir -p $(HEADERDIR)/binutils/opcodes
	cp $(MAO_OPCODES_HEADERS) $(HEADERDIR)/binutils/opcodes
	mkdir -p $(HEADERDIR)/binutils/opcode
	cp $(MAO_INCLUDE_OPCODE_HEADERS) $(HEADERDIR)/binutils/opcode

stamp-bin:
	test -d $(BINDIR) || mkdir -p $(BINDIR)
	touch stamp-bin

stamp-obj-$(TARGET):
	test -d $(OBJDIR) || mkdir -p $(OBJDIR)
	touch stamp-obj-$(TARGET)

clean : 
	-rm -f $(OBJDIR)/*
	-rm -f $(BINDIR)/mao-$(DEVPREFIX)$(TARGET) $(PLUGIN_TARGETS) $(BINDIR)/mao

allclean : 
	-rm -rf ../obj-*
	-rm -rf $(BINDIR)
	-rm -f stamp-bin
	-rm -f stamp-obj-*

-include $(DEPS)
