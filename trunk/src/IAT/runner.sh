##############################
#
#!/bin/bash
#
# IAT: Runner script - automatically compiles ASM files, 
# runs the executable through pfmon, 
# and stores results
#
# NOTE: This script is divided into three parts
# which include runner.sh, runnerII.sh, and cleaner.sh
# 
# NOTE: If root@yrnw8 requires login and breaks script,
# be sure to have the following code in your .profile textfile:
#		
#     if [ -x /usr/local/scripts/ssx-agents ] ; then
#       eval `/usr/local/scripts/ssx-agents $SHELL`
#     fi
#
##############################

#move scripts over to prod machine
chmod 755 runnerII.sh
chmod 755 cleaner.sh
rsync --rsh="ssh -x" cleaner.sh root@yrnw8:/export/hda3/smohapatra > /dev/null
rsync --rsh="ssh -x" runnerII.sh root@yrnw8:/export/hda3/smohapatra > /dev/null

#TODO(smohapatra) command line args for Directory and pfmon command
#input: file to test
echo "Please enter the name of the directory you wish to test"
read DIRECTORY

if [ ! -d $DIRECTORY ]; then
  echo "$DIRECTORY : does not exist"
  exit 1
  fi

#create results dir and store location
if [ ! -d ${DIRECTORY}_Results ]; then
  mkdir "${DIRECTORY}_Results"
  fi
cd "${DIRECTORY}_Results"
pwd > location.txt
mv ./location.txt ../${DIRECTORY}
cd ..

#index.txt is the file where the names of all the
#ASM files are stored; iterate through all names
#cp ./runnerII.sh ./${DIRECTORY}/
cd ./"$DIRECTORY"
INDEXFILE=index.txt
if [ ! -f $INDEXFILE ]; then
  echo "$INDEXFILE: does not exist"
  exit 1
elif [ ! -r $INDEXFILE ]; then
  echo "$INDEXFILE: cannot read"
  exit 2
fi

#compile all files
for CURRENTFILE in $(cat $INDEXFILE); do
  if [ ! -f $CURRENTFILE ]; then
    echo "$CURRENTFILE: does not exist"
    echo $CURRENTFILE >> failedtocompile.txt
    continue
  else
    EXECUTABLEFILE=test${CURRENTFILE//".s"/}
    gcc "$CURRENTFILE" -o "$EXECUTABLEFILE".exe
    if [ $? -ne 0 ]; then
      echo $CURRENTFILE >> failedtocompile.txt
      continue
    else
      echo $CURRENTFILE >> successfullycompiled.txt
    fi
    echo "$EXECUTABLEFILE" >> executablefiles.txt
  fi
done

if [ -f ./successfullycompiled.txt ]; then
  cp ./successfullycompiled.txt ../${DIRECTORY}_Results
fi
if [ -f ./failedtocompile.txt ]; then
  mv ./failedtocompile.txt ../${DIRECTORY}_Results
fi

#pfmon commands
PFMONCOMMAND=" "
EXIT=""
    
echo "Enter the event you would like to test for in all caps
   (include all necessary parameters and flags as needed).
   Press enter to exit."
read PFMONCOMMAND 
echo "$PFMONCOMMAND" > command.txt

#tar all files to be rsynced over
echo "$USER" > user.txt
hostname -s > machine.txt
tar -czvf executablefiles.tar ./*.exe > /dev/null
tar -czvf infofiles.tar ./*.txt > /dev/null

rsync --rsh="ssh -x" executablefiles.tar root@yrnw8:/export/hda3/smohapatra > /dev/null
rsync --rsh="ssh -x" infofiles.tar root@yrnw8:/export/hda3/smohapatra > /dev/null
ssh -x root@yrnw8 /export/hda3/smohapatra/runnerII.sh > /dev/null 

#rsync results back
rsync --rsh="ssh -x" root@yrnw8:/export/hda3/smohapatra/results.tar ../${DIRECTORY}_Results/ > /dev/null
cd ../${DIRECTORY}_Results/
tar -zxvf results.tar > /dev/null
rm -rf ../${DIRECTORY}_Results/results.tar
cd ../${DIRECTORY}

#clean up
rm -rf *.tar
rm -rf *.exe
rm -rf command.txt
rm -rf executablefiles.txt
rm -rf location.txt
rm -rf machine.txt
rm -rf successfullycompiled.txt
rm -rf user.txt
ssh -x root@yrnw8 'sh /export/hda3/smohapatra/cleaner.sh > /dev/null'

exit 0
