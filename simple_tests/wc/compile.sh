echo "Running pass"
clang -emit-llvm -o wc.bc -c wc.c
opt -insert-path-profiling wc.bc -o wc.pp.bc || { echo "Failed to path profile"; exit 1; }
llc wc.pp.bc -o wc.pp.s
g++ -o wc.profile wc.pp.s /usr/local/lib/libprofile_rt.so
./wc.profile input.txt
opt -load /vagrant/eecs583-static-estimation/static-estimation-pass/build/static-estimation/libStaticEstimator.so -path-profile-loader -path-profile-loader-file=llvmprof.out -StaticEstimatorPass wc.bc || { echo "Failed to build features"; exit 1; }

# Just to test feature extractor
#clang -emit-llvm -S -o wc.bc wc.c
#opt < wc.bc > wc.se.bc -load /vagrant/eecs583-static-estimation/static-estimation-pass/build/static-estimation/libFeatureExtractorHarness.so -FeatureExtractorHarnessPass
