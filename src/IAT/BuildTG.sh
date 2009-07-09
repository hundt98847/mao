g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Assembly.d" -MT"Assembly.d" -o"Assembly.o" "./Assembly.cpp"
g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"TestGenerator.d" -MT"TestGenerator.d" -o"TestGenerator.o" "./TestGenerator.cpp"
g++  -o"TestGenerator"  ./Assembly.o ./TestGenerator.o
