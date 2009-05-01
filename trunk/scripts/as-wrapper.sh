#!/bin/bash
#
# $Id$
# Copyright 2009 Google Inc. All Rights Reserved.
# Author: eraman@google.com (Easwaran Raman)
# Wrapper to as that calls mao if required before calling original as
#

save_temps=0
as_args=( )
mao_args=( )
invoke_mao=0
output_file='a.out'
input_files=()



#######################################
# Parse arguments to as to separate mao and as arguments,
# identify input and output files and recognize --save-temps
#
# Globals:
#   save_temps
#   invoke_mao
#   input_files
#   output_file
#   as_args
#   mao_args
# Arguments:
#   arguments processed by getopt
# Returns:
#   None
#######################################

function parse_args() {
  while [[ $# -gt 0 ]]; do
    case $1
    in
      '--save-temps')
        save_temps=1
        shift 1
      ;;
      
      '--mao')
        mao_args[${#mao_args[*]}]="--mao=$2"
        invoke_mao=1
        shift 2
      ;;
  
      '-o')
        output_file=$2
        shift 2
      ;;
  
      '--')
        shift
        input_files=$@
        break;
      ;;
  
      *)
        mao_args[${#mao_args[*]}]=$1
        as_args[${#as_args[*]}]=$1
        shift
    esac
  done
}


function main()  {
  # Use getopt to separate out options from input files. Makes it easy
  # to find inputs to as
  local dir_name=`dirname $0`
  local mao_bin=${dir_name}/mao
  local as_bin=${dir_name}/as-orig
  #long options used by as
  local as_long_opts="alternate,a::,al::debug-prefix-map:,defsym:,dump-config,emulation:,execstack,noexecstack,\
fatal-warnings,gdwarf-2,gdwarf2,gen-debug,gstabs,gstabs+,hash-size:,help,itbl:,keep-locals,listing-lhs-width:,\
listing-lhs-width2:,listing-rhs-width:,MD:,mri,nocpp,no-warn,reduce-memory-overheads,statistics,\
strip-local-absolute,version,verbose,target-help,traditional-format,warn,mao:,save-temps,32,64,divide,march:,\
mtune:,mnemonic:,msyntax:,mindex-reg,mnaked-reg,mold-gcc,msse2avx,msse-check:"
  
  #short options used by as
  local as_short_opts="JKLMRWZa::Dfg::I:o:vwXt:kVQ:sqn"

  local mao_output_file='a.mao.s'
  if [[ $output_file != "a.out" ]]; then
    mao_output_file=${output_file/%.o/.mao.s}
  fi
  
  local preprocessed_args="$(getopt  -u -a -o${as_short_opts} -l${as_long_opts} -- $@)"
  if [[ $? = 1 ]]; then
    echo "$0: Unable to parse options" >&2
    exit 1
  elif [[ $? = 3 ]]; then
    echo "$0: Internal error in getopt" >&2
    exit 1
  fi
  parse_args ${preprocessed_args}
  if [[ ${invoke_mao} = 1 ]]; then
    echo "${mao_bin} ${mao_args[*]} ${input_files} -o ${mao_output_file}"
    if [[ ! -x "${mao_bin}" ]]; then
      echo "Unable to execute mao binary : ${mao_bin}"
      exit 1
    fi
    #${mao_bin} ${mao_args[*]} ${input_files} -o ${mao_output_file}
    echo "${as_bin} ${as_args[*]} ${mao_output_file} -o ${output_file}"
    if [[ ! -x "${as_bin}" ]]; then
      echo "Unable to execute as binary : ${as_bin}"
      exit 1
    fi
    #${as_bin} ${as_args[*]} ${mao_output_file} -o ${output_file}
    if [[ ${save_temps} = 0 ]]; then
      echo "rm ${mao_output_file}"
      # rm {mao_output_file}
    fi
  else
    echo $0 $@
  fi
}

main $@  
