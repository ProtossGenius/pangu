##Tail
.PHONY: prebuild linux tests test qrun install clean format

BUILD_DIR ?= build
BUILD_TYPE ?= Release
BUILD_JOBS ?= 8

prebuild:
	# smist -exts ".cpp,.c,.h,.hpp,.cc"
	python3 ./format
	smdcatalog

dlex:
	sudo mv /var/lib/apport/coredump/* ./build/core.dump 
	cd build && echo 'bt' | gdb pangu-lexer core.dump | less
debug:
	sudo mv /var/lib/apport/coredump/* ./build/core.dump 
	cd build && echo 'bt' | gdb pangu-grammer core.dump | less
qrun:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(BUILD_JOBS) && ./pangu-grammer ../test_datas/grammer/func_code.pgl

linux:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) .. && make -j$(BUILD_JOBS)

tests: linux
	bash all_test.sh

test: tests

install:

clean:

format:
	python3 ./format
