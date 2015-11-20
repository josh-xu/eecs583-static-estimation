clang -Xclang -load -Xclang ../../static-estimation-pass/build/static-estimation/libStaticEstimation.* wc.c
clang -emit-llvm -S -o output.bc -c wc.c
