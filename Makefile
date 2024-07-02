##Tail
prebuild:
	# smist -exts ".cpp,.c,.h,.hpp,.cc"
	python3 ./format
	smdcatalog	

debug:
	mv /var/lib/apport/coredump/* ./build/core.dump 
	cd build && gdb pangu core.dump
qrun:
	rm -f /var/lib/apport/coredump/*
	mkdir -p build
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j24 && ./pangu ../test_datas/grammer/grammer.pgl
test:
	mkdir -p build 
	cd build && cmake .. && make -j24 
	sh all_test.sh

install:

clean:

format:
	python3 ./format
