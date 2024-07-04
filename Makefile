##Tail
prebuild:
	# smist -exts ".cpp,.c,.h,.hpp,.cc"
	python3 ./format
	smdcatalog	

dlex:
	sudo mv /var/lib/apport/coredump/* ./build/core.dump 
	cd build && echo 'bt' | gdb pangu-lexer core.dump | vim -R -
debug:
	sudo mv /var/lib/apport/coredump/* ./build/core.dump 
	cd build && echo 'bt' | gdb pangu core.dump | vim -R -
qrun:
	sudo rm -f /var/lib/apport/coredump/*
	mkdir -p build
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j24 && ./pangu ../test_datas/grammer/grammer.pgl
test:
	sudo rm -f /var/lib/apport/coredump/*
	mkdir -p build 
	cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j24 
	sh all_test.sh

install:

clean:

format:
	python3 ./format
