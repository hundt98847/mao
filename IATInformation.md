# Background and Description #
The gcc compiler does not easily provide or have access to some information about events at the hardware level for instructions; some examples are micro-operations and instructions retired per instruction. This information is useful to know for optimization; in the case of a tool such as MAO, this information can be used to show which instructions are faster on given architectures for certain addressing modes, which allows for optimization at the hardware level.

IAT's goal, therefore, is to easily provide this information for all instructions on all addressing modes for given architectures.

# Quality #
In order to maintain usable and robust code, IAT will attempt to maintain the following quality guidelines.
  * Efficient - Uses most effective algorithms and minimizes expensive operations
  * Readable - Code adheres to Google Code Style Guidelines and Conventions
  * Well Documented - Project's structure and operation is well documented both inside and outside of code
  * Flexible - All segments of the project should remain resistant to errors and unexpected results.

# Design Plan #
For simplicity in explanation, we'll discuss an example. Let's say we're looking at the ADD [R1](https://code.google.com/p/mao/source/detail?r=1), [R2](https://code.google.com/p/mao/source/detail?r=2) instruction in x86 and we're looking for micro-operations (uops).

In order to get an estimate of how uops are executed in the ADD [R1](https://code.google.com/p/mao/source/detail?r=1), [R2](https://code.google.com/p/mao/source/detail?r=2) instruction, we would run the instruction several hundreds of times and obtain an average number of total uops. From there, we would divide the number of uops retired by the number of ADDs we ran in order to get a number of uops per instruction.

The project was split into three parts: A generator, a runner, and an analysis tool

# Generator #
This part of the tool creates assembly files that resemble the following pseudocode:


---

for(a certain number of user defined iterations)
> INSTRUCTION OPERANDS <br />
> INSTRUCTION OPERANDS <br />
> INSTRUCTION OPERANDS <br />
> INSTRUCTION OPERANDS <br />
> INSTRUCTION OPERANDS <br />
> > (so on for however many times the user decides) <br />
> > ...

---

The produced assembly files are then stored in an automatically generated directory, which is named by the time and date at which it was generated.


# Runner #
This is a script that takes the generated assembly files, compiles the .s files, then runs them through [http://perfmon2.sourceforge.net/pfmon_usersguide.html[pfmon](pfmon.md)] to look at the CPU events that occur while the ASM file is run (events are specified by the user). These results are then stored in an automatically generated directory.

# Analyzer #
Having produced the total number of events, the analysis tool takes these results and puts them in machine digestible form to be used by MAO or any other tool that needs this information.

# How to Use IAT #
**Setup**
You can compile the Test Generator and Analyzer by using the make command:


> user@machine:~$ make

**Test Generator**
You will then get an executable called TestGenerator.  The following command line arguments are availible and optional:

  * --instructions=value: The number of instructions contained within the body loop of each assembly test.
  * --iterations=value: The number of times each assembly test should iterate over the body instructions.
  * --verbose: Display status messages for each processed operation, operand, and test.
  * --help: Display this help message.

Example Executions:
  * user@machine:~$ ./TestGenerator --instructions=100 --iterations=50 --verbose
  * user@machine:~$ ./TestGenerator --help

After the Test Generator completes, a new directory will appear in your current directory; the name of this directory is a time stamp of when the Test Generator began execution. (Ex: Mon\_Aug\_10\_16\_46\_16\_2009).  This directory name must be passed to the next segment of IAT.

**Runner**
There are several parameters that must be defined in the command line, and this can be done with the following command line arguments.

  * --testdir, -t: The name of the directory to be tested.
  * --cpuevent, -c: The name of the CPU event/pfmon command to test for
  * --pfmonmachine, -m: The name of the machine that has pfmon on it
  * --pfmonuser, -u: The username to use on the pfmon machine
  * --pfmondirectory, -d: The path of the directory to use on the pfmon machine

Example Executions:
  * user@machine:-$ sh runner.sh --testdir=Mon\_Aug\_10\_16\_46\_16\_2009 -c INSTRUCTIONS\_RETIRED --pfmonmachine=yrnw8 -u root -d /export/hda3/smohapatra

Other flags:

  * --silent: Status messages will not be printed
  * --help, -h: Instructions for how to run the runner will be displayed.

If a parameter is undefined in the command line arguments when using the runner, the script will terminate automatically. The script will use the files in the given test directory and will generate a directory called testdir\_results (Ex: Input - Mon\_Aug\_10\_16\_46\_16\_2009 Output - Mon\_Aug\_10\_16\_46\_16\_2009\_results). This will become the results input for the Analyzer.

**Analyzer**
The Analyzer is responsible for aggregating all the test results into a single results.txt file.
The following arguments are required:

  * --results=directory: The directory containing the output generated by the Runner script.

The following arguments are optional:
  * --instructions=     The number of instructions contained within the body loop of each assembly test.
  * --iterations=       The number of times each assembly test iterates over the body instructions.
  * Should the --instructions= or --iterations= command line arguments not be provided, this information will be read from test\_set.dat within the results directory.
  * --verbose           Display status messages for each processed result.
  * --help              Display this help message.

Example Executions:
  * user@machine:-$ ./Analyzer --results=testdir\_results
  * user@machine -$ ./Analyzer --help

The Analyzer places its results in a text file called results.txt in testdir\_results.

# Important Notes #
  * It's not recommended to run the Test Generator or any segment of IAT on a network-based file system.  Thousands of files must are generated and utilized by the tool, which makes it inappropriate for anything but a local disk.  Keep it to /usr/local/!
  * It's also not recommended to run the Test Generator within an Eclipse workspace.  Eclipse has the nasty habit of having to index all the contents of a workspace on a regular basis (every time eclipse is launched, every time you build), and when it hits a folder of many thousands of files, it doesn't react so well.
  * Currently, the methodology of the tool creates a very flat file system structure.  This makes dealing with these files difficult, so you may need to individually remove groups of files within test directories to clear tests.  Future work may consist of separating instructions into individual directories.
  * If any of the parameters in the runner script are undefined, the script will automatically terminate.

For further instructions, please check the README, or contact us!
  * casey.burkhardt@villanova.edu
  * smohapa3@illinois.edu