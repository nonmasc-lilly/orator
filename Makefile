all: main.c
	gcc main.c -o main -std=c89
build: main.c
	gcc main.c -o main -std=c89
	mkdir build_out
	cp ./main ./build_out/main
	cp ./home.html ./build_out/home.html
	mkdir build_out/available
