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

#command line args - see README
if [[ "$1" != "" ]]; then
  DIRECTORY="$1"
fi

if [[ "$2" != "" ]]; then
  PFMONCOMMAND="$2"
fi

if [[ "$3" != "" ]]; then
  PFMONMACHINE="$3"
fi

if [[ "$4" != "" ]]; then
  PFMONUSER="$4"
fi

if [[ "$5" != "" ]]; then
  PFMONDIR="$5"
fi

#define variables for sshing into pfmon machine
if [[ ! -n "${PFMONMACHINE}" ]]; then
  echo 'Please enter the machine to test ASM files on (has pfmon on it)'
  read PFMONMACHINE
fi

if [[ ! -n "${PFMONUSER}" ]]; then
  echo 'Please enter the username on the machine to test ASM files on'
  read PFMONUSER
fi

if [[ ! -n "${PFMONDIR}" ]]; then
  echo 'Please enter directory to copy files to for testing'
  read PFMONDIR
fi

ssh -xt "${PFMONUSER}@${PFMONMACHINE}" "cd ${PFMONDIR}" > /dev/null
if [[ $? -ne 0 ]]; then
  echo "${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR} not available."
  exit 1
fi

#move scripts over to prod machine
echo "Relocating execution scripts."
chmod o+x runnerII.sh
chmod o+x cleaner.sh
rsync --rsh="ssh -x" cleaner.sh ${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR} \
> /dev/null
rsync --rsh="ssh -x" runnerII.sh ${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR} \
> /dev/null

#input: file to test
if [[ ! -n "$DIRECTORY" ]]; then
  echo 'Please enter the name of the directory you wish to test'
  read DIRECTORY
fi

if [[ ! -d $DIRECTORY ]]; then
  echo "$DIRECTORY : does not exist"
  exit 1
fi

#format handling
if [[ "${DIRECTORY}" == */ ]]; then
  DIRECTORY=$(echo "${DIRECTORY%/}")
fi

#create results dir and store location
echo "Creating results directory"
if [[ ! -d ${DIRECTORY}_Results ]]; then
  mkdir "${DIRECTORY}_Results"
fi
cp "${DIRECTORY}/test_set.dat" "${DIRECTORY}_Results" 
if [ $? -ne 0 ]; then
  echo "Copy of necessary data failed."
  exit 1
fi

#index.txt is the file where the names of all the
#ASM files are stored; iterate through all names
#cp ./runnerII.sh ./${DIRECTORY}/
cd ./"${DIRECTORY}"
INDEXFILE=index.txt
if [[ ! -f $INDEXFILE ]]; then
  echo "$INDEXFILE: does not exist"
  exit 2
elif [[ ! -r $INDEXFILE ]]; then
  echo "$INDEXFILE: cannot read"
  exit 3
fi

#Compile all files using make
echo "Compiling tests"
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
  mv ./successfullycompiled.txt ../${DIRECTORY}_Results
fi
if [[ -f ./failedtocompile.txt ]]; then
  mv ./failedtocompile.txt ../${DIRECTORY}_Results
fi

#pfmon commands
if [[ ! -n "$PFMONCOMMAND" ]]; then
  PFMONCOMMAND=" "
  EXIT=""
    
  echo "Enter the event you would like to test for in all caps
   (include all necessary parameters and flags as needed).
   Press enter to exit."
  read PFMONCOMMAND 
fi

if [[ ! -n "$PFMONCOMMAND" ]]; then
  echo "No pfmon command was specified"
  exit 1
fi 

#tar all files to be rsynced over
echo "Gatering successfuly generated tests for analysis"
tar -czvf allfiles.tar *.exe "./executablefiles.txt" > /dev/null
echo "Transferring test binaries for analysis"
rsync --rsh="ssh -x" allfiles.tar \
${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR} > /dev/null
ssh -x ${PFMONUSER}@${PFMONMACHINE} \
"sh ${PFMONDIR}/runnerII.sh ${PFMONDIR} ${PFMONCOMMAND}"

#rsync results back if ssh was successful
if [[ $? -eq 0 ]]; then
  echo "Transferring execution results."
  rsync --rsh="ssh -x" ${PFMONUSER}@${PFMONMACHINE}:${PFMONDIR}/results.tar \
../${DIRECTORY}_Results/ > /dev/null
  if [[ $? -eq 0 ]]; then
    cd ../${DIRECTORY}_Results/
    echo "Distributing execution results."
    tar -zxvf results.tar > /dev/null
    rm -rf ../${DIRECTORY}_Results/results.tar
    cd ../${DIRECTORY}
  fi
else
  echo "ssh into ${PFMONUSER}@${PFMONMACHINE} failed."
  continue
fi

#clean up
rm allfiles.tar
ssh -x ${PFMONUSER}@${PFMONMACHINE} \
"sh ${PFMONDIR}/cleaner.sh ${PFMONDIR} > /dev/null"

exit 0
