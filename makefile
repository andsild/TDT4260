m5 = ../m5/build/ALPHA_SE/m5.opt
pf = ../prefetcher/prefetcher.cc
$(m5): $(pf); cd .. && ./compile.sh

test: $(m5); cd .. && ./test_prefetcher.py
gtest: test_prefetcher.cc
	g++  test_prefetcher.cc -o gtestme.o
