#!/bin/bash
#
# $Id$
# Copyright 2009 Google Inc. All Rights Reserved
# Author: smohapatra@google.com (Sweta Mohapatra)
#
# IAT: cleaner.sh - part of IAT runner script
# Removes all files on prod server

PFMONDIR=$1
cd ${PFMONDIR}
if [[ $? -eq 0 ]]; then
  rm -rf *
fi

exit 0
