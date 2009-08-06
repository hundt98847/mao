CFLAGS=-g -c
LDFLAGS=-g -o

all: TestGenerator Analyzer

TestGenerator: TestGenerator.o Assembly.o Operation.o Operand.o
	g++ $(LDFLAGS) "TestGenerator" Operation.o Operand.o Assembly.o TestGenerator.o

TestGenerator.o: TestGenerator.cc TestGenerator.h
	g++ $(CFLAGS) TestGenerator.cc

Assembly.o: Assembly.cc Assembly.h
	g++ $(CFLAGS) Assembly.cc

Operation.o: Operation.cc Operation.h
	g++ $(CFLAGS) Operation.cc

Operand.o: Operand.cc Operand.h
	g++ $(CFLAGS) Operand.cc

Analyzer: Analyzer.o Result.o
	g++ $(LDFLAGS) "Analyzer" Result.o Analyzer.o

Analyzer.o: Analyzer.cc Analyzer.h
	g++ $(CFLAGS) Analyzer.cc

Result.o: Result.cc Result.h
	g++ $(CFLAGS) Result.cc

clean:
	rm *.o TestGenerator Analyzer
