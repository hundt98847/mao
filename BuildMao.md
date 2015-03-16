# Build instructions #

MAO reuses code from the GNU Binutils (currently v2.22) for parsing an input assembly
file. Before building MAO, it is necessary to download Binutils, and patch the sources using a provided patch (see instructions below).

MAO currently supports i686 and x86\_64 targets on Linux and Mac OS X (only tested on Leopard). Please note any host-specific instructions below.

```
#
#=========================================================
# MAO Sources
#=========================================================

# Set target to i686-linux or x86_64
TARGET=x86_64-linux

# On MacOS, please set the following required flag:
# BINUTILS_CONF_FLAGS=-disable-nls
# otherwise, clear the flag:
BINUTILS_CONF_FLAGS=

mkdir MAO
cd MAO

# If you haven't downloaded the source for mao:
# See http://code.google.com/p/mao/source/checkout for more info
svn checkout https://mao.googlecode.com/svn/trunk/ . --username your.name

#=========================================================
# GNU Binutils sources
#=========================================================
# Fetch and patch binutils sources.
#    (note: sometimes updates are being uploaded to the ftp
#           please check for latest versions)
#
#    (note: Some Apple OS X systems don't have wget installed. Either
#           install it, or set this alias:
#           alias wget="curl -O"
#
wget http://ftp.gnu.org/gnu/binutils/binutils-2.22.tar.bz2
bunzip2 binutils-2.22.tar.bz2
tar xvf binutils-2.22.tar
(cd binutils-2.22 && patch -p1  < ../data/binutils-2.22-mao.patch)

# Configure modified binutils.
#  note: make -j 4 spawn parallel make, 4 processes. Increase
#        or decrease, according to your needs and system
(mkdir binutils-2.22-obj-${TARGET}
cd binutils-2.22-obj-${TARGET}
../binutils-2.22/configure --target=${TARGET} ${BINUTILS_CONF_FLAGS}
make -j 4)

#=========================================================
# Build MAO
#=========================================================
# Please check the Makefile, as it contains a reference to the binutils
# directory. Update, if necessary
#
cd src

make -j 4 TARGET=${TARGET}
#or
make -j 4 TARGET=${TARGET} OS=MacOS
```

Now, goto RunMao for information on how to use the generated MAO binary in MAO/bin/ (named mao-${TARGET}).