#!/bin/bash

# input is the benchmark we want to try (and optionally the number of lines in the assembly to test)
# Simple verification script written by martint

# Input is either .c, .cpp, cc, or .s file.

#  Some of the more important variables used below
#  
#    MAO_ROOT      = Environment variable must be set before running this script
#    WORKDIR       = Directory used for holding all files used in the verification
#    IN_FILE       = Input file
#    S_FILE        = name of assembly file inside the WORKDIR
#    LINES         = Number of lines to verify in the assembly file (0 means all lines)
#                    0 is the default, but can be overridden from the command line

fatal() {
  echo "$@" 1>&2
  exit 1
}

function isNumeric(){ 
  echo "$@" | grep -q -v "[^0-9]"
}

function validate_target() {
  local l_target=$1

  supported_targets=( i686-linux x86_64-linux )

  supported=0
  for supported_target in ${supported_targets[@]}
  do
    if [ ${l_target}x == ${supported_target}x ]; then
      supported=1
    fi
  done
  
  if [ ${supported} -eq 0 ]; then
    fatal "Unsupported target : " ${l_target}
  fi

  echo $l_target
}

USAGE="Usage: mao_verify.sh target infile [number of lines to verify]"

TARGET=$(validate_target $1)

# Check command line parameters
if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  echo "${USAGE}";
  exit 1;
fi;
# Get the number of lines to verify
if [ -z $3 ]; then
  LINES=0
else
  if isNumeric "$3"; then
    LINES="$3"
  else
     echo "${USAGE}";
     exit 1;    
  fi
fi

IN_FILE=$2

CC=gcc
BINUTILS=binutils-2.19.1
AS=${MAO_ROOT}/${BINUTILS}-obj-${TARGET}/gas/as-new
READELF=${MAO_ROOT}/${BINUTILS}-obj-${TARGET}/binutils/readelf
OBJDUMP=${MAO_ROOT}/${BINUTILS}-obj-${TARGET}/binutils/objdump
MAO=${MAO_ROOT}/bin/mao-${TARGET}

if [ "${MAO_ROOT}x" == "x" ]; then
  echo "Please set MAO_ROOT to point to the root directory of the mao project"
  exit 1;
fi

if [ ! -d "${MAO_ROOT}" ]; then
  echo "Please set MAO_ROOT to point to a valid directory. Currently its set to ${MAO_ROOT}"
  exit 1;
fi


if [ ! -f "${MAO}" ]; then
  echo "Unable to find executable: ${MAO}"
  exit 1;
fi

if [ ! -f "${AS}" ]; then
  echo "Unable to find assembler: ${AS}"
  exit 1;
fi

if [ ! -f "${OBJDUMP}" ]; then
  echo "Unable to find objdump: ${OBJDUMP}"
  exit 1;
fi

if [ ! -f "${READELF}" ]; then
  echo "Unable to find readelf: ${READELF}"
  exit 1;
fi

# Create a workdir. 
#WORKDIR=Verify.${RANDOM}
WORKDIR=`mktemp -d Verify.XXXXXX` || (echo "Unable to create directiry" && exit 1)

# Static variables
FILENAME=`basename "${IN_FILE}"`
EXTENSION="${FILENAME##*.}"
BENCHMARK="${FILENAME%.*}"
S_FILE="${WORKDIR}/${BENCHMARK}.s"
MAO_FILE="${WORKDIR}/${BENCHMARK}.mao"
TMP_O_FILE="${WORKDIR}/tmp.o"

# Check the type of file
case "${EXTENSION}" in
  "S"|"s"                    ) ;;
  *                          ) echo "Not a valid file type" && exit;;
esac 

if [ ! -f "${IN_FILE}" ]; then
  echo "File does not exist: ${IN_FILE}";
  exit;
fi
# Now create the S_FILE
if [ ${LINES} -eq 0 ]; then
  cp "${IN_FILE}" "${S_FILE}"
fi;
if [ ${LINES} -gt 0 ]; then
  head -n ${LINES} "${IN_FILE}" > "${S_FILE}"
fi;

echo "Processing: ${IN_FILE}"

# Run it trough mao
# Currently does not use gcc as a wrapper, since I dont have a good way to select
# the target platform in gcc (yet).
${MAO} --mao=ASM=o["${MAO_FILE}"] "${S_FILE}"

if [ $? -ne 0 ]; then
  echo "Error creating assembly";
  exit;
fi

# # Generate object files from both
${AS} -o "${S_FILE}.o"   "${S_FILE}" 
${AS} -o "${MAO_FILE}.o" "${MAO_FILE}" 

DIFF_RESULT=`diff "${S_FILE}".o "${MAO_FILE}".o`

if [ $? -ne 0 ]; then
#   # There was a difference! Print out the disassembly
  echo "Difference found"
  ${READELF} -a "${S_FILE}".o >  "${S_FILE}".o.readelf
  ${READELF} -a "${MAO_FILE}".o >  "${MAO_FILE}".o.readelf
  ${OBJDUMP} -d "${S_FILE}".o > "${S_FILE}".o.diss
  ${OBJDUMP} -d "${MAO_FILE}".o > "${MAO_FILE}".o.diss
  echo " Use the following commands to debug:"
  echo "sdiff -w 200 \"${S_FILE}.o.readelf\" \"${MAO_FILE}.o.readelf\""
  echo "sdiff -w 200 \"${S_FILE}.o.diss\" \"${MAO_FILE}.o.diss\""
  exit 1
else
  echo -n "${BENCHMARK} is OK"
  if [ ${LINES} -gt 0 ]; then
    echo -n " for the first ${LINES} lines"
  fi
  echo  
  rm "${S_FILE}" "${MAO_FILE}" "${S_FILE}.o" "${MAO_FILE}.o"
  rmdir "${WORKDIR}"
fi
exit 0
