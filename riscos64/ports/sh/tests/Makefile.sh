SHELL = ./sh
.PHONY: all
all:
	echo built > out_txt; cat out_txt
	for f in a b c; do echo item $$f; done
	test -f out_txt && echo exists
	x=$$(echo sub); echo got $$x
	@echo "val: $(shell echo from-shell)"
	./tprog arg1 "quoted arg" | ./tprog -i
	rm -f out_txt; test ! -f out_txt && echo removed
