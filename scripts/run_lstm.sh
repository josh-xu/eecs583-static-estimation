#!/bin/bash

if [ $# -eq 0 ]
  then
    echo "nothing supplied"
    exit 1
fi

benchmark_name=$1
benchmark_exec=$2
benchmark_load=$3

# create run command
cmd="./${benchmark_exec}.profile ${benchmark_load}"

# set library paths
source paths.sh
echo ${benchmark_exec}
echo ${so_module_path_lstm}

# save current directory
CURRENT_DIR=$(pwd)

echo "*** moving to benchmark src directory"
cd ../benchmarks/${benchmark_name}

echo "*** cleaning previous shit"
rm *.s
rm *.profile
rm *.pp.bc

echo "*** compiling to single .bc file"
make

echo "*** inserting path profiling"
opt -insert-path-profiling ${benchmark_exec}.bc -o ${benchmark_exec}.pp.bc
llc ${benchmark_exec}.pp.bc -o ${benchmark_exec}.pp.s

echo "*** running profiling"
g++ -o ${benchmark_exec}.profile ${benchmark_exec}.pp.s ${libprofile_rt_path}
eval "$cmd"

echo "*** extracting features"
opt -load=${so_module_path_lstm} -path-profile-loader -path-profile-loader-file=llvmprof.out -LSTMStaticEstimatorPass ${benchmark_exec}.bc

echo "*** moving CSV file"
mv feature_output.csv $CURRENT_DIR/features_files_lstm/${benchmark_exec}.csv

echo "*** moving back"
cd $CURRENT_DIR

