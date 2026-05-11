all: build

build: compilation

compilation: prepare
	echo "Компиляция файлов"
	g++ app_payload64_burst512.cpp $(shell pkg-config --cflags --libs libdpdk) -o ./build/app_p64_b512 -march=native
	g++ app_payload1500_burst512.cpp $(shell pkg-config --cflags --libs libdpdk) -o ./build/app_p1500_b512 -march=native
	echo "Компиляция завершена"

prepare:
	mkdir -p ./build

clear:
	rm -r ./build

clear_hugepages: 
	sudo python3 usertools/dpdk-hugepages.py -p 2M --setup 0G
	sudo python3 usertools/dpdk-hugepages.py -p 1G --setup 0G

run_p64_b512:
	sudo ./build/app_p64_b512

run_p1500_b512:
	sudo ./build/app_p1500_b512

hugepages_p2M_setup1g:
	sudo python3 usertools/dpdk-hugepages.py -p 2M --setup 1G 

hugepages_p1G_setup1g:
	sudo python3 usertools/dpdk-hugepages.py -p 1G --setup 1G


