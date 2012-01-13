#!/bin/bash -x
#
# $Id$
# Copyright 2009 Google Inc. All Rights Reserved
# Author: smohapatra@google.com (Sweta Mohapatra)
#
# IAT: Runner script - automatically compiles ASM files, 
# runs the executable through pfmon, and stores results
#
# NOTE: This script is divided into three parts
# which include runner.sh, runnerII.sh, and cleaner.sh
# 
# NOTE: If root@hostname requires login and breaks script,
# be sure to have the following code in your .profile textfile:
#		
#    if [ -x /usr/local/scripts/ssx-agents ] ; then
#      eval `/usr/local/scripts/ssx-agents $SHELL`
#    fi

#######################################
# Print status message
#
# Arguments:
#   Status message 
# Returns:
#   None
#######################################

function statmessage() {
  if [[ ! "${SILENT}" ]]; then
    echo "${@}" 
  fi
}

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

#######################################
# Parse arguments to define test directory, pfmon command,
# machine w/pfmon on it, username on pfmon machine, and
# directory to use on pfmon machine (See README).
#
# Arguments:
#   arguments processed by getopt
# Returns:
#   None
#######################################

function parse_args() {
  while [[ $# -gt 1 ]]; do
    case $1
    in
      '-t')
        DIRECTORY=$2
        shift 2
      ;;

      '-c')
        PFMONCOMMAND=$2
        shift 2
      ;;

      '-m')
        PFMONMACHINE=$2
        shift 2
      ;;

      '-u')
        PFMONUSER=$2
        shift 2
      ;;

      '-d')
        PFMONDIR=$2
        shift 2
      ;;

      '-h')
        echo "Running the Runner
You will need to put define the following arguments using these flags:
  
  --testdir, -t
    The name of the directory to be tested.

  --cpuevent, -c
    The name of the CPU event/pfmon command to test for

  --pfmonmachine, -m
    The name of the machine that has pfmon on it

  --pfmonuser, -u
    The username to use on the pfmon machine

  --pfmondirectory, -d
    The path of the directory to use on the pfmon machine

Should one of these arguments not be defined, the script will
automatically terminate.

Optional flag(s):

  --silent, -s
    Silent mode: runner will not print status messages

The output from the Runner will be placed in a directory named
identical to that of the Test Generator output directory with "_results"
appended.  Pfmon output may be found in the files within that directory.
Sample pfmon commands may be found within the "pfmoncommands.txt" file. For
further questions or information on pfmon, please refer to the following
website for details: http://perfmon2.sourceforge.net/pfmon_usersguide.html

SPECIAL NOTE: If authentication to the remote server via SSH requires
password authentication, the script will fail.  Instead, be sure that you
have automatic authentication setup between the local and remote hosts." 
        exit 0
      ;;

      '-s')
        SILENT=1
        shift 1
      ;;

      '--testdir')
        DIRECTORY=$2
        shift 2
      ;;
     
      '--cpuevent')
        PFMONCOMMAND=$2
        shift 2
      ;;

      '--pfmonmachine')
        PFMONMACHINE=$2
        shift 2
      ;;
 
      '--pfmonuser')
        PFMONUSER=$2
        shift 2
      ;;

      '--pfmondir')
        PFMONDIR=$2
        shift 2
      ;;
      
      '--help')
        echo "Running the Runner
You will need to put define the following arguments using these flags:
  
  --testdir, -t
    The name of the directory to be tested.

  --cpuevent, -c
    The name of the CPU event/pfmon command to test for

  --pfmonmachine, -m
    The name of the machine that has pfmon on it

  --pfmonuser, -u
    The username to use on the pfmon machine

  --pfmondirectory, -d
    The path of the directory to use on the pfmon machine

Should one of these arguments not be defined, the script will
automatically terminate.

Optional flag(s):

  --silent, -s
  Silent mode: runner will not print status messages.

The output from the Runner will be placed in a directory named
identical to that of the Test Generator output directory with "_results"
appended.  Pfmon output may be found in the files within that directory.
Sample pfmon commands may be found within the "pfmoncommands.txt" file. For
further questions or information on pfmon, please refer to the following
website for details: http://perfmon2.sourceforge.net/pfmon_usersguide.html

SPECIAL NOTE: If authentication to the remote server via SSH requires
password authentication, the script will fail.  Instead, be sure that you
have automatic authentication setup between the local and remote hosts." 
       exit 0
     ;;

     '--silent')
       SILENT=1
       shift 1
     ;;

      *)
        fatal "Illegal argument."
    esac
  done
}

#command line args - see README
LONGOPTS="testdir:,cpuevent:,pfmonmachine:,pfmonuser:,pfmondir:,help,silent"
SHORTOPTS="t:c:m:u:d:hs"
PROCESSEDARGS="$(getopt -u -a -o${SHORTOPTS} -l${LONGOPTS} -- $@)"
RETVAL=$?
if [[ ${RETVAL} -eq 1 ]]; then
  fatal "$0: Unable to parse options" 
elif [[ ${RETVAL} -eq 2 ]]; then
  fatal "$0: getopt does not understand its own parameters" 
elif [[ ${RETVAL} -eq 3 ]]; then
  fatal "$0: Internal error in getopt" 
fi

parse_args ${PROCESSEDARGS}

#format handling
if [[ "${DIRECTORY}" == */ ]]; then
  DIRECTORY=$(echo "${DIRECTORY%/}")
fi

if [[ ! "${DIRECTORY}" ]]; then
  fatal "Directory to test undefined."
elif [[ ! "${PFMONCOMMAND}" ]]; then
  fatal "Command for pfmon undefined."
elif [[ ! "${PFMONUSER}" ]]; then
  fatal "Username for machine w/pfmon undefined."
elif [[ ! "${PFMONMACHINE}" ]]; then
  fatal "Machine w/pfmon not given."
elif [[ ! "${PFMONDIR}" ]]; then
  fatal "Directory on pfmon machine undefined."
fi

ssh -xt "${PFMONUSER}@${PFMONMACHINE}" "cd ${PFMONDIR}" > /dev/null
if [[ $? -ne 0 ]]; then
  fatal "${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR} not available."
fi

#move scripts over to prod machine
statmessage "Relocating execution scripts."
chmod o+x runnerII.sh
chmod o+x cleaner.sh
rsync --rsh="ssh -x" cleaner.sh ${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR} \
> /dev/null
rsync --rsh="ssh -x" runnerII.sh ${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR} \
> /dev/null

#input: file to test
if [[ ! -d $DIRECTORY ]]; then
  fatal "$DIRECTORY : does not exist"
fi

#create results dir and store location
statmessage "Creating results directory."
if [[ ! -d ${DIRECTORY}_results ]]; then
  mkdir "${DIRECTORY}_results"
fi
cp "${DIRECTORY}/test_set.dat" "${DIRECTORY}_results" 
if [ $? -ne 0 ]; then
  fatal "Copy of necessary data failed."
fi

#index.txt is the file where the names of all the
#ASM files are stored; iterate through all names
#cp ./runnerII.sh ./${DIRECTORY}/
cd ./"${DIRECTORY}"
INDEXFILE=index.txt
if [[ ! -f $INDEXFILE ]]; then
  fatal "$INDEXFILE: does not exist."
elif [[ ! -r $INDEXFILE ]]; then
  fatal "$INDEXFILE: cannot read."
fi

#Compile all files using make
statmessage "Compiling tests"
make -k -s -j 16 2> /dev/null

#Determine which files compiled successfully
for CURRENTFILE in $(cat "${INDEXFILE}"); do
  EXECUTABLEFILE="test_${CURRENTFILE//".s"/.exe}"
  if [[ -f ${EXECUTABLEFILE} ]]; then
    echo ${CURRENTFILE} >> successfullycompiled.txt
    echo ${EXECUTABLEFILE//".exe"/} >> executablefiles.txt
  else
    echo ${CURRENTFILE} >> failedtocompile.txt
  fi
done

if [[ -f ./successfullycompiled.txt ]]; then
  mv ./successfullycompiled.txt ../${DIRECTORY}_results
fi
  
if [[ -f ./failedtocompile.txt ]]; then
  mv ./failedtocompile.txt ../${DIRECTORY}_results
fi

#tar all files to be rsynced over
statmessage "Gathering successfully generated tests for analysis."
tar -czvf allfiles.tar *.exe "./executablefiles.txt" > /dev/null
statmessage "Transferring test binaries for analysisi."
rsync --rsh="ssh -x" allfiles.tar \
${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR} > /dev/null
ssh -x ${PFMONUSER}@${PFMONMACHINE} \
"sh ${PFMONDIR}/runnerII.sh ${PFMONDIR} ${PFMONCOMMAND}"

#rsync results back if ssh was successful
if [[ $? -eq 0 ]]; then
  statmessage "Transferring execution results."
  rsync --rsh="ssh -x" ${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR}/results.tar \
../${DIRECTORY}_results/ > /dev/null
  if [[ $? -eq 0 ]]; then
    cd ../${DIRECTORY}_results/
    statmessage "Distributing execution results."
    tar -zxvf results.tar > /dev/null
    rm -rf ../${DIRECTORY}_results/results.tar
    cd ../${DIRECTORY}
  fi
else
  statmessage "ssh into ${PFMONUSER}@${PFMONMACHINE} failed."
  continue
fi

#clean up
statmessage "Clean up."
rm allfiles.tar
ssh -x ${PFMONUSER}@${PFMONMACHINE} \
"sh ${PFMONDIR}/cleaner.sh ${PFMONDIR} > /dev/null"

exit 0
