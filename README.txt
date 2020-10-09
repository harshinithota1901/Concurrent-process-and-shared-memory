1. Compile with make
gcc -Wall -ggdb -c shared.c
gcc -Wall -ggdb master.c shared.o -o master
gcc -Wall -ggdb palin.c shared.o -o palin

2. Run the program
$ ./master -n 7
