#!/usr/bin/python2.4

"""This script runs MAO on the set of files listed in the input argument file(s).
Lines starting with # are comments. If the verification script finds an error
the script exists with the error message."""

import os
import subprocess
import sys
import tempfile

def _RunCheck(command, stdin=None, stdout=None, stderr=None, env=None):
  try:
    child = subprocess.Popen(command,
                             stdin=stdin, stdout=stdout, stderr=stderr,
                             env=env)
    (_, err) = child.communicate()
    err = child.wait()
    if err != 0:
      sys.stderr.write("Error running command: %s\n" % " ".join(command))
      sys.exit(err)
  except OSError, e:
    sys.stderr.write("Error running command: %s\n" % " ".join(command))
    print e
    sys.exit(1)

def _VerifyFile(file):
  print "Testing \"" + file + "\":"
  _RunCheck(["./mao_verify.sh", "x86_64", file]);

# Parses a text-file with one assembly-file per line, and runs mao in it.
# Lines starting with a hash-symbol (#) are ignored.
def _Process(infile):
  f = open(infile, 'r')
  for filename in f:
    if not filename.startswith('#'):
      _VerifyFile(filename.strip())
  f.close()

def main(argv):
  for arg in argv:
    _Process(arg)

if __name__ == "__main__":
  main(sys.argv[1:])
