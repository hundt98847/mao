#!/usr/bin/python2.4
# -*- mode: python -*-

"""This script will run MAO on an assembly file and compare the result with the
original assembly file by checking the resulting assembled object files from
both assembly files. Temporary files are created during execution, and removed
before exit. The script assumes that as-orig exists in the current
directory, and that mao is available as ../bin/mao-TARGET."""

import os
import sys
import re
import subprocess
import tempfile

class RunError(Exception):
  def __init__(self, returncode, command, root):
    Exception.__init__(self)
    self.returncode = returncode
    self.command = command
    self.root = root

  def __str__(self):
    line1 = "Error running command: %s\n" % " ".join(self.command)
    if self.root:
      line2 = str(self.root)
      return "%s%s\n" % (line1, line2)
    else:
      return line1

def _RunCheck(command, stdin=None, stdout=None, stderr=None, env=None):
  try:
    child = subprocess.Popen(command,
                             stdin=stdin, stdout=stdout, stderr=stderr,
                             env=env)
    (output, _) = child.communicate()
    err = child.wait()
    if err != 0:
      raise RunError(err, command, None)
    return output
  except OSError, e:
    raise RunError(1, command, e)

def _Run(command, stdin=None, stdout=None, stderr=None, env=None):
  child = subprocess.Popen(command,
                           stdin=stdin, stdout=stdout, stderr=stderr,
                           env=env)
  child.communicate()
  err = child.wait()
  return err


# Remove files in the inputlist and exit with exit code 1
def _Fail(files_to_remove):
  for file in files_to_remove:
    _RunCheck(["rm", "-f", file])
  sys.exit(1)

def main(argv):
  in_file = ""
  if len(argv) != 3:
    print "Usage: " + argv[0] + " TARGET inputfile"
    sys.exit(1)

  # Holds the names of the generated temporary files
  generated_files = []

  target  = argv[1]
  in_file = argv[2]
  basedir = os.path.join(os.path.dirname(argv[0]))

  # run mao on the input .s file
  (fd, mao_tempfile) = tempfile.mkstemp(suffix=".mao")
  os.close(fd)
  generated_files.append(mao_tempfile)
  cmd = [os.path.join(basedir, "../bin/mao-" + target), "--mao=ASM=o[" + \
         str(mao_tempfile) + "]", in_file]
  mao_result = _Run(cmd)

  if mao_result != 0:
    # On error, cleanup and return
    _Fail(generated_files)
  else:
    # Create object from from original .s file
    (fd, o_tempfile) = tempfile.mkstemp(suffix=".o")
    os.close(fd)
    generated_files.append(o_tempfile)
    as_cmd = [os.path.join(basedir, "as-orig"), "-o", o_tempfile, in_file]
    ret = _Run(as_cmd)
    if ret != 0:
      _Fail(generated_files)

    # Create object file from .mao file
    (fd, mao_o_tempfile) = tempfile.mkstemp(suffix=".mao.o")
    os.close(fd)
    generated_files.append(mao_o_tempfile)
    as_cmd = [os.path.join(basedir, "as-orig"), "-o", mao_o_tempfile,\
              mao_tempfile]
    ret = _Run(as_cmd)
    if ret != 0:
      _Fail(generated_files)

    # Run diff on the two object files
    diff_cmd = ["diff", o_tempfile, mao_o_tempfile]
    diff_result = _Run(diff_cmd)

    # Remove any temporary files
    for generated_file in generated_files:
      _RunCheck(["rm", "-f", generated_file])

    ret_val = diff_result

  sys.exit(ret_val)

if __name__ == "__main__":
  main(sys.argv)
