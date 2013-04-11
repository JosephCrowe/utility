all:
	make -C trimnl
	make -C nanorc
	make -C pipeserve

clean:
	make -C trimnl clean
	make -C nanorc clean
	make -C pipeserve clean

.phony: all clean
