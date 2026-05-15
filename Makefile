all: build

build: compilation

compilation: prepare
	echo "Компиляция файлов"
	g++ main.cpp $(shell pkg-config --cflags --libs libdpdk) -o ./build/main.bin -march=native
	echo "Компиляция завершена"

prepare:
	mkdir -p ./build

clear:
	rm -r ./build

clear_hugepages: 
	sudo python3 usertools/dpdk-hugepages.py -p 2M --setup 0G
	sudo python3 usertools/dpdk-hugepages.py -p 1G --setup 0G

run:
	sudo ./build/main.bin

hugepages_p2M_setup1g:
	sudo python3 usertools/dpdk-hugepages.py -p 2M --setup 1G 

hugepages_p1G_setup1g:
	sudo python3 usertools/dpdk-hugepages.py -p 1G --setup 1G


