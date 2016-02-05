m5 = ../m5/build/ALPHA_SE/m5.opt
pf = ../prefetcher/prefetcher.cc
all: test
$(m5): $(pf); cd .. && ./compile.sh

test: $(m5); cd .. && ./test_prefetcher.py
