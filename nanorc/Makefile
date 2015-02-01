all: haskell.nanorc java.nanorc prolog.nanorc js.nanorc erlang.nanorc \
     lingo.nanorc makefile.nanorc lparse.nanorc

lparse.nanorc: lparse.expand.nanorc
	./expand.py lparse.expand.nanorc > lparse.nanorc

makefile.nanorc: makefile.expand.nanorc
	./expand.py makefile.expand.nanorc > makefile.nanorc

lingo.nanorc: lingo.expand.nanorc
	./expand.py lingo.expand.nanorc > lingo.nanorc

erlang.nanorc: erlang.expand.nanorc erlang.common.nanorc
	./expand.py erlang.expand.nanorc > erlang.nanorc

haskell.nanorc: haskell.common.nanorc haskell.expand.nanorc
	./expand.py haskell.expand.nanorc > haskell.nanorc

java.nanorc: java.expand.nanorc
	./expand.py java.expand.nanorc > java.nanorc

prolog.nanorc: prolog.common.nanorc prolog.expand.nanorc
	./expand.py prolog.expand.nanorc > prolog.nanorc

js.nanorc: js.expand.nanorc
	./expand.py js.expand.nanorc > js.nanorc

.phony: all
