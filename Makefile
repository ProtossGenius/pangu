##Tail
prebuild:
	# smist -exts ".cpp,.c,.h,.hpp,.cc"
	python3 ./format
	smdcatalog	

debug:

qrun:
	mkdir -p build
	cd build && cmake .. && make -j$(nproc) && ./pangu
test:

install:

clean:

format:
	python3 ./format
