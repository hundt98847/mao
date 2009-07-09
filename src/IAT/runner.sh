#bin/bash
#
# Mock runner script - practice run
# for major functions needed
# 
#NOTE: Even though script moves to ./SongsDir directory, when it ends, it remains in directory 
#      where script is
#NOTE: If root@yrnw8 requires login and breaks script, be sure to have the following code in 
#      your .profile textfile:
#		if [ -x /usr/local/scripts/ssx-agents ] ; then
#   			eval `/usr/local/scripts/ssx-agents $SHELL`
#		fi

chmod 755 runnerII.sh

#input: file to test
echo "Please enter the name of the directory you wish to test"
read DIRECTORY

if [ ! -d $DIRECTORY ]; then
  echo "$DIRECTORY : does not exist"
  exit 1
  fi

#create results dir and store location
if [ ! -d ${DIRECTORY}Results ]; then
  mkdir "${DIRECTORY}Results"
  fi
cd "${DIRECTORY}Results"
pwd > location.txt
mv ./location.txt ../${DIRECTORY}
cd ..

#index.txt is the file where the names of all the
#ASM files are stored; iterate through all names
cp ./runnerII.sh ./${DIRECTORY}/
cd ./"$DIRECTORY"
INDEXFILE=index.txt
if [ ! -f $INDEXFILE ]; then
  echo "$INDEXFILE: does not exist"
  exit 1
elif [ ! -r $INDEXFILE ]; then
  echo "$INDEXFILE: cannot read"
  exit 2
fi

for CURRENTFILE in $(cat $INDEXFILE); do
  if [ ! -f $CURRENTFILE ]; then
    echo "$CURRENTFILE: does not exist"
    continue
  else
    gcc "$CURRENTFILE" -o test${CURRENTFILE//".s"/}
    if [ $? -ne 0 ]; then
      echo $CURRENTFILE >> failedtocompile.txt
      continue
    else
      echo $CURRENTFILE >> successfullycompiled.txt
    fi
    EXECUTABLEFILE=test${CURRENTFILE//".s"/}

    #pfmon commands loop
    PFMONCOMMAND=" "
    EXIT=""
    
    while [ "$PFMONCOMMAND" != "$EXIT" ]
    do
      echo "Enter the event you would like to test for in all caps. Press enter to exit."
      read PFMONCOMMAND
      if [ "$PFMONCOMMAND" != "$EXIT" ]; then
        echo "$PFMONCOMMAND" >> ${EXECUTABLEFILE}commands.txt
        fi
    done


    echo "$EXECUTABLEFILE" > executable.txt
    echo "$USER" > user.txt
    hostname -s > machine.txt
    rsync machine.txt root@yrnw8:/export/hda3/smohapatra > /dev/null
    rsync location.txt root@yrnw8:/export/hda3/smohapatra > /dev/null
    rsync user.txt root@yrnw8:/export/hda3/smohapatra > /dev/null
    rsync executable.txt root@yrnw8:/export/hda3/smohapatra > /dev/null
    rsync $EXECUTABLEFILE root@yrnw8:/export/hda3/smohapatra > /dev/null
    rsync ${EXECUTABLEFILE}commands.txt root@yrnw8:/export/hda3/smohapatra > /dev/null
    rsync runnerII.sh root@yrnw8:/export/hda3/smohapatra > /dev/null
    ssh root@yrnw8 /export/hda3/smohapatra/runnerII.sh > /dev/null

    #rsync results back from prod machine
    rsync root@yrnw8:/export/hda3/smohapatra/${EXECUTABLEFILE}Results.txt ../${DIRECTORY}Results/ > /dev/null
    rsync root@yrnw8:/export/hda3/smohapatra/${EXECUTABLEFILE}failedcommands.txt ../${DIRECTORY}Results/ > /dev/null

    if [ -f ./successfullycompiled.txt ]; then
      mv ./successfullycompiled.txt ../${DIRECTORY}Results/
    fi
    if [ -f ./failedtocompile.txt ]; then
      mv ./failedtocompile.txt ../${DIRECTORY}Results/
    fi
    rm executable.txt
    rm user.txt
    rm machine.txt
    rm location.txt
    rm ${EXECUTABLEFILE}commands.txt
    rm $EXECUTABLEFILE
    rm runnerII.sh
  fi

done

exit 0
