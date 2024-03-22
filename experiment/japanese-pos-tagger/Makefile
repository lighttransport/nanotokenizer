all:
	clang++ -o train -fsanitize=address -I../common -g -O1 train.cc

t:
	./train dev-dict dev.JAG

