#!/bin/bash

#move to directory where files are
#and remove files for cleanup after
#variables are set
cd /export/hda3/smohapatra/
chmod 755 *.txt
EXECUTABLE=$(cat executable.txt)
USER=$(cat user.txt)
LOCATION=$(cat location.txt)
MACHINE=$(cat machine.txt)
RETURNLOCATION="$USER"@"$MACHINE":"$LOCATION"
COMMANDSFILE=${EXECUTABLE}commands.txt

#pfmon commands loop
for CURRENTCOMMAND in $(cat $COMMANDSFILE); do
  pfmon -e "$CURRENTCOMMAND" ./"${EXECUTABLE}" > /dev/null
  if [ $? -ne 0 ]; then
    echo "$CURRENTCOMMAND: command failed"
    echo "$CURRENTCOMMAND" >> ${EXECUTABLE}failedcommands.txt
  else
    pfmon -e "$CURRENTCOMMAND" ./"$EXECUTABLE" >> "$EXECUTABLE"Results.txt
  fi
done

if [ ! -f ${EXECUTABLE}failedcommands.txt ]; then
  echo "No failed commands!" > ${EXECUTABLE}failedcommands.txt
  fi

rm -rf executable.txt
rm -rf user.txt
rm -rf location.txt
rm -rf machine.txt
rm -rf ${EXECUTABLE}commands.txt
rm -rf $EXECUTABLE

exit 0
