#clang -Xclang -load -Xclang ../../static-estimation-pass/build/static-estimation/libPassProfiler* wc.c
clang -emit-llvm -S -o wc.bc wc.c
opt -load /vagrant/eecs583-static-estimation/static-estimation-pass/build/static-estimation/libPassProfiler.so < wc.bc > wc.se.bc
