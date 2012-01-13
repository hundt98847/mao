#!/bin/bash -x
#
# $Id$
# Copyright 2009 Google Inc. All Rights Reserved
# Author: smohapatra@google.com (Sweta Mohapatra) 
#
# IAT: runnerII.sh - part of IAT runner script
# Move to directory on prod machine where files
# are and remove files for cleanup after
# variables are set

#######################################
# Error message to be printed on failure
#
# Arguments:
#   Error message 
# Returns:
#   None
#######################################

function error() {
  echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: ${@}" >&2
}

#######################################
# Prints error message and exits program
#
# Arguments:
#   Error message 
# Returns:
#   Return value greater than 0 for failure
#######################################

function fatal() {
  error ${@}
  exit 1
}

#check for needed tar files
PFMONDIR=$1
COMMAND=$2
cd ${PFMONDIR}

chmod o+x *
if [[ -f allfiles.tar ]]; then
  tar -zxvf allfiles.tar > /dev/null
else
  fatal 'allfiles.tar: not found'
fi

#pfmon loop
INDEXFILE=./executablefiles.txt

if [[ ! -d results/ ]]; then 
  mkdir results/
fi

for EXECUTABLEFILE in $(cat $INDEXFILE); do
  LOGENTRY=${EXECUTABLEFILE:5}
  LOGENTRY=${LOGENTRY//".exe"/}.s
  pfmon -e "$COMMAND" ./"${EXECUTABLEFILE}".exe > success.txt
  if [[ $? -ne 0 ]]; then
    echo ${LOGENTRY} >> ./results/failedexecutables.txt
  else
    echo ${LOGENTRY} >> ./results/successfulexecution.txt
    cat success.txt >> ./results/"$EXECUTABLEFILE"_results.txt
  fi
done

#tar result files to rsync back
cd ./results
tar -czvf results.tar ./* > /dev/null
mv results.tar ${PFMONDIR}
cd ..

exit 0
