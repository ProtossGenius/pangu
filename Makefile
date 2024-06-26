##Tail
prebuild:
	# smist -exts ".cpp,.c,.h,.hpp,.cc"
	python3 ./format
	smdcatalog	

debug:

qrun:
	mkdir -p build
	cd build && cmake .. && make -j24 && ./pangu ../Application.cpp
test:
	mkdir -p build 
	cd build && cmake .. && make -j24 
	sh all_test.sh

install:

clean:

format:
	python3 ./format
