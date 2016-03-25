#!/bin/bash

if [ $# -eq 0 ]
  then
    echo "Please specify csv file as the argument"
    echo "Format: <benchmark folder name>, <benchmark bc file>, <benchmark runtime input & args>" 
    exit 1
fi

cat $1 | while read line
do
#    echo "Text read from file: $line"

benchmark_name=$(echo $line | cut -d "," -f1)
benchmark_exec=$(echo $line | cut -d "," -f2)
benchmark_load=$(echo $line | cut -d "," -f3)

#benchmark_name=$1
#benchmark_exec=$2
#benchmark_load=$3

#echo $benchmark_load

# create run command
cmd="./${benchmark_exec}.profile ${benchmark_load} &> prog.out"
#cmd="./${benchmark_exec}.profile ${benchmark_load}"



# set library paths
source paths.sh
#echo ${benchmark_exec}
#echo ${so_module_path_lstm}

#so_module_path=(/opt/compiler_project/steve/static-estimation-pass/build/static-estimation/libStaticProfiler.so)
profiler_so_module_path_lstm=(/opt/compiler_project/steve/static-estimation-pass-dev/build/static-estimation/libLSTMStaticProfiler.so)
spoofer_so_module_path_lstm=(/opt/compiler_project/steve/static-estimation-pass-dev/build/static-estimation/libLSTMProfileSpoofer.so)

# save current directory
cd ..
CURRENT_DIR=$(pwd)
echo ""
echo "*** Processing benchmark src directory: " $benchmark_name " ***"
cd benchmarks_daniel/${benchmark_name}
bench_dir=$(pwd)

echo "---> Step 4: Re-Optimizing based on static profile..."
time opt -load=${spoofer_so_module_path_lstm} -profile-spoofer -block-placement ${benchmark_exec}.bc -o ${benchmark_exec}.reop.bc
cat err_out | grep "error"
echo "-> Feature extraction finished. Output saved to ${bench_dir}/reop.out"

#echo "---> Step 4: Running profiling..."
#g++ -o ${benchmark_exec}.profile ${benchmark_exec}.pp.s ${libprofile_rt_path} 
#eval "$cmd"
#echo "-> Execution finished. Output saved to: ${bench_dir}/prog.out"

#echo "--> Moving back"
cd $CURRENT_DIR

done
