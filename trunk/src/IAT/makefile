all: TestGenerator Analyzer

TestGenerator: TestGenerator.o Assembly.o Operation.o Operand.o
	g++ -o "TestGenerator" Operation.o Operand.o Assembly.o TestGenerator.o

TestGenerator.o: TestGenerator.cc TestGenerator.h
	g++ -c TestGenerator.cc

Assembly.o: Assembly.cc Assembly.h
	g++ -c Assembly.cc

Operation.o: Operation.cc Operation.h
	g++ -c Operation.cc

Operand.o: Operand.cc Operand.h
	g++ -c Operand.cc

Analyzer: Analyzer.o Result.o
	g++ -o "Analyzer" Result.o Analyzer.o

Analyzer.o: Analyzer.cc Analyzer.h
	g++ -c Analyzer.cc

Result.o: Result.cc Result.h
	g++ -c Result.cc

clean:
	rm *.o TestGenerator Analyzer
