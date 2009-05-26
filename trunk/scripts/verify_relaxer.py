#!/usr/bin/python2.4
# -*- mode: python -*-

"""This script will run MAO on an assembly file and compare the functionsizes
calculated by the relaxer with the sizes reported by readelf on the assembled
file. All temporary files are deleted. If verbose_error is True, a line is
printed for each function that is unsuccessfully verified. If no such function
is found by MAO, the rror message is

"functionname ERROR: Unable to find function in MAO."

If the sizes differ the error message is

"functionname     readelf_size mao_size"

Additionally, the script assumes that in the directory containing the script
there are binaries named mao and as-orig which are the MAO and original GAS
binaries, respectively. It also calls readelf to read the symbols of the object
file."""


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


################################################################################
# Run mao on the inputfile and return a map with the reported function sizes.
# Assumes that command "mao" exists in the current directory.
################################################################################
def GetMaoSizes(inputfile, basedir):
  # Run mao and get the sizes from the report
  cmd = [os.path.join(basedir, "mao"), "--mao:ASM=o[/dev/null]", \
           "--mao=TEST=relax[1],cfg[0],lsg[0]", "--mao=RELAX=stat[1]", inputfile]
  output = _RunCheck(cmd, stdout=subprocess.PIPE)
  mao_function_sizes = {}
  for outputline in output.split('\n'):
    outwords = outputline.split()
    if len(outwords) == 4 and outwords[0] == 'MaoRelax' and \
       outwords[1] == 'functionsize':
      if not outwords[2] in mao_function_sizes:
        mao_function_sizes[outwords[2]] = outwords[3]
  return mao_function_sizes


################################################################################
# Assemble the inputfile with the command as-orig and uses readelf to get the
# reported functionsizes from the object files.
# Assumes that command "as-orig" exists in the current directory.
################################################################################
def GetReadelfSizes(inputfile, basedir):
  (fd, o_tempfile) = tempfile.mkstemp(suffix=".o")
  os.close(fd)
  as_cmd = [os.path.join(basedir, "as-orig"), "-o", o_tempfile, inputfile]
  _RunCheck(as_cmd)
  readelf_cmd = ["readelf", "--wide", "-s", o_tempfile]
  readelf_output = _RunCheck(readelf_cmd, stdout=subprocess.PIPE)
  readelf_function_sizes = {}
  for outputline in readelf_output.split('\n'):
    # Use regexp to match the interesting lines
    match = re.search(r'[0-9]+ FUNC +', outputline)
    if match:
      outwords = outputline.split()
      readelf_function_sizes[outwords[7]] = outwords[2]
  _RunCheck(["rm", "-f", o_tempfile])
  return readelf_function_sizes



def main(argv):
  in_file = ""
  if len(argv) != 2:
    print "Usage: " + argv[0] + " inputfile"
    sys.exit(1)

  # If True, print a line for each unsuccessfully verified function
  verbose_errors = True
  # If True, print a line for each verified function
  verbose_all    = True

  in_file = argv[1]

  mao_sizes     = GetMaoSizes(in_file, os.path.join(os.path.dirname(argv[0])))
  readelf_sizes = GetReadelfSizes(in_file, os.path.join(os.path.dirname(argv[0])))

  found_error    = False
  # Iterate over the function found in the object file
  for (key, val) in readelf_sizes.items():
    function_ok = False
    if mao_sizes.get(key, None) == None:
      # Function not found in MAO
      if verbose_errors or verbose_all:
        print 'ERROR %(functionname)-60s Unable to find function in MAO.' % \
              {'functionname': key }
    else:
      # Verify calculated size
      if int(val) == int(mao_sizes[key]):
        # Match
        function_ok = True
        if verbose_all:
          print 'CORRECT %(functionname)-60s %(readelf_size)5d' % \
                {'functionname': key, 'readelf_size': int(val)}
      else:
        # Mismatch
        if verbose_errors or verbose_all:
          print 'ERROR %(functionname)-60s %(readelf_size)5d %(mao_size)5d' % \
                {'functionname': key, \
                 'readelf_size': int(val), \
                 'mao_size': int(mao_sizes[key])}
    # Make sure errors are logged, and return value is correct
    if not function_ok:
      found_error = True

  # Return
  if found_error:
    sys.exit(1)
  else:
    sys.exit(0)
if __name__ == "__main__":
  main(sys.argv)
