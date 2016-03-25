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
cmd_prof="time ./${benchmark_exec}.profile -llvmprof-output blockprof.out ${benchmark_load}"
cmd_exe="time ./${benchmark_exec}.dy.exe ${benchmark_load}"
#cmd="./${benchmark_exec}.profile ${benchmark_load}"
cmd_3_1="opt -insert-block-profiling ${benchmark_exec}.bc -o ${benchmark_exec}.bp.bc"
cmd_3_2="llc ${benchmark_exec}.bp.bc -o ${benchmark_exec}.bp.s"
cmd_4_1="g++ -o ${benchmark_exec}.profile ${benchmark_exec}.bp.s ${libprofile_rt_path} -lm"
cmd_5_1="time opt -profile-loader -profile-info-file=blockprof.out -block-placement ${benchmark_exec}.bc -o ${benchmark_exec}.dy.bc"
cmd_6_1="llc ${benchmark_exec}.dy.bc -o ${benchmark_exec}.dy.s"
cmd_7_1="g++ -o ${benchmark_exec}.dy.exe ${benchmark_exec}.dy.s ${libprofile_rt_path} -lm"

# set library paths
source paths.sh
#echo ${benchmark_exec}
#echo ${so_module_path_lstm}

#so_module_path=(/opt/compiler_project/steve/static-estimation-pass/build/static-estimation/libStaticProfiler.so)
#profiler_so_module_path_lstm=(/opt/compiler_project/steve/static-estimation-pass-dev/build/static-estimation/libLSTMStaticProfiler.so)
#spoofer_so_module_path_lstm=(/opt/compiler_project/steve/static-estimation-pass-dev/build/static-estimation/libLSTMProfileSpoofer.so)

# save current directory
cd ..
CURRENT_DIR=$(pwd)
echo ""
echo "*** Processing benchmark src directory: " $benchmark_name " ***"
cd benchmarks/${benchmark_name}
bench_dir=$(pwd)


echo "--> Step 1: Cleaning benchmark directory..."
rm *.s &> /dev/null
rm *.profile &> /dev/null
rm *.pp.bc &> /dev/null
rm *.bp.bc &> /dev/null
rm blockprof.out &> /dev/null
make clean &> /dev/null
echo ""

echo "---> Step 2: Compiling to bitcode"
make &> err_out
cat err_out | grep "Error"
echo ""

echo "---> Step 3: Inserting path profiling..."
eval "$cmd_3_1"
eval "$cmd_3_2"
echo ""

echo "---> Step 4: Running profiling..."
g++ -o ${benchmark_exec}.profile ${benchmark_exec}.bp.s ${libprofile_rt_path} -lm
eval "$cmd_prof"
echo "-> Execution finished."
echo ""

echo "---> Step 5: Re-Optimizing based on dynamic profile..."
eval "$cmd_5_1"
echo "-> Dynamic PGO finished."
echo ""

echo "---> Step 6: Compiling re-optimized bitcode..."
eval "$cmd_6_1"
echo ""

echo "---> Step 7: Running profiling..."
eval "$cmd_7_1"
eval "$cmd_exe"
echo "-> Execution finished."
echo ""

echo "--> Moving back"
cd $CURRENT_DIR

done

