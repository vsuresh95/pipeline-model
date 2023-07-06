APP=pipeline

compile:
	g++ -std=c++11 $(APP).cpp -o $(APP).out

run:
	./$(APP).out

clean:
	rm -rf *.out *.log

all: clean compile run