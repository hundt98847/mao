g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Operation.d" -MT"Operation.d" -o"Operation.o" "../Operation.cc"
g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Operand.d" -MT"Operand.d" -o"Operand.o" "../Operand.cc"
g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Assembly.d" -MT"Assembly.d" -o"Assembly.o" "./Assembly.cc"
g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"TestGenerator.d" -MT"TestGenerator.d" -o"TestGenerator.o" "./TestGenerator.cc"
g++  -o"TestGenerator" ./Operation.o ./Operand.o ./Assembly.o ./TestGenerator.o
