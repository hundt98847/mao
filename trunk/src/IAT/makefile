CFLAGS=-g -c -O3
LDFLAGS=-g -o

all: TestGenerator Analyzer

TestGenerator: TestGenerator.o Assembly.o
	g++ $(LDFLAGS) "TestGenerator" TestGenerator.o

TestGenerator.o: TestGenerator.cc
	g++ $(CFLAGS) TestGenerator.cc

Assembly.o: Assembly.h
	g++ $(CFLAGS) Assembly.h

Operation.o: Operation.h
	g++ $(CFLAGS) Operation.h

Operand.o: Operand.h
	g++ $(CFLAGS) Operand.h

Analyzer: Analyzer.o
	g++ $(LDFLAGS) "Analyzer" Analyzer.o

Analyzer.o: Analyzer.cc
	g++ $(CFLAGS) Analyzer.cc

Result.o: Result.h
	g++ $(CFLAGS) Result.h

clean:
	rm *.o *.gch TestGenerator Analyzer
