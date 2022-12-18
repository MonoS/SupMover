all: supmover

supmover: main.o
	g++ -o supmover main.o

main.o: main.cpp
	g++ -std=c++17 -fexceptions -O2 -Wall -Wextra -c main.cpp -o main.o

clean:
	rm -f *.o supmover

.PHONY: clean
