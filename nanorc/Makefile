all: haskell.nanorc java.nanorc prolog.nanorc js.nanorc erlang.nanorc \
    lingo.nanorc makefile.nanorc lparse.nanorc wb_asp.nanorc wb_haskell.nanorc

wb_haskell.nanorc: wb_haskell.expand.nanorc haskell.syntax.nanorc
	./expand.py wb_haskell.expand.nanorc > wb_haskell.nanorc

wb_asp.nanorc: wb_asp.expand.nanorc lparse.syntax.nanorc
	./expand.py wb_asp.expand.nanorc > wb_asp.nanorc

lparse.nanorc: lparse.expand.nanorc lparse.syntax.nanorc
	./expand.py lparse.expand.nanorc > lparse.nanorc

makefile.nanorc: makefile.expand.nanorc
	./expand.py makefile.expand.nanorc > makefile.nanorc

lingo.nanorc: lingo.expand.nanorc
	./expand.py lingo.expand.nanorc > lingo.nanorc

erlang.nanorc: erlang.expand.nanorc erlang.common.nanorc
	./expand.py erlang.expand.nanorc > erlang.nanorc

haskell.nanorc: haskell.*.nanorc
	./expand.py haskell.expand.nanorc > haskell.nanorc

java.nanorc: java.expand.nanorc
	./expand.py java.expand.nanorc > java.nanorc

prolog.nanorc: prolog.common.nanorc prolog.expand.nanorc
	./expand.py prolog.expand.nanorc > prolog.nanorc

js.nanorc: js.expand.nanorc
	./expand.py js.expand.nanorc > js.nanorc

.phony: all
