echo "Running pass"
clang -emit-llvm -S -o wc.bc wc.c
#opt < wc.bc > wc.se.bc -load /vagrant/eecs583-static-estimation/static-estimation-pass/build/static-estimation/libStaticEstimator.so -StaticEstimatorPass
#clang -emit-llvm -S -o wc.bc wc.c
opt < wc.bc > wc.se.bc -load /vagrant/eecs583-static-estimation/static-estimation-pass/build/static-estimation/libFeatureExtractorHarness.so -FeatureExtractorHarnessPass
