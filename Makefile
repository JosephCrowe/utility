all:
	make -C trimnl
	make -C nanorc

clean:
	make -C trimnl clean
	make -C nanorc clean

.phony: all clean
