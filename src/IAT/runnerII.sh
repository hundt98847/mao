#############################################
#
#!/bin/bash
#
# IAT: runnerII.sh - part of IAT runner script
# Move to directory on prod machine where files
# are and remove files for cleanup after
# variables are set
#
#############################################

#check for needed tar files
cd /export/hda3/smohapatra/
chmod 755 *
if [ -f infofiles.tar ]; then
  tar -zxvf infofiles.tar
else
  echo "infofiles.tar: not found"
  exit 1
fi
if [ -f executablefiles.tar ]; then
  tar -zxvf executablefiles.tar
else
  echo "executablefiles.tar: not found"
  exit 1
fi

#pfmon loop
INDEXFILE=./executablefiles.txt
COMMAND=$(cat command.txt)

mkdir results/

for EXECUTABLEFILE in $(cat $INDEXFILE); do
  pfmon -e "$COMMAND" ./"${EXECUTABLEFILE}".exe > /dev/null
  
  #TODO(smohapatra): find a way to check exit status w/o calling pfmon twice
  if [ $? -ne 0 ]; then
    echo "$COMMAND: command failed." 
    echo "$COMMAND" >> "$EXECUTABLEFILE"failedcommands.txt
  else
    pfmon -e "$COMMAND" ./"${EXECUTABLEFILE}".exe >> "$EXECUTABLEFILE"results.txt
    echo "$COMMAND" >> "$EXECUTABLEFILE"successfulcommands.txt
  fi
 
  if [ -f "$EXECUTABLEFILE"failedcommands.txt ]; then
    mv "$EXECUTABLEFILE"failedcommands.txt ./results/
  fi
  
  if [ -f "$EXECUTABLEFILE"successfulcommands.txt ]; then
    mv "$EXECUTABLEFILE"successfulcommands.txt ./results/
  fi

  if [ -f "$EXECUTABLEFILE"results.txt ]; then
    mv "$EXECUTABLEFILE"results.txt ./results/
  fi
done

#tar result files to rsync back
cd ./results
tar -czvf results.tar ./*
mv results.tar ..
cd ..

exit 0
