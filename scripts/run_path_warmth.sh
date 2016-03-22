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

# save current directory
cd ..
CURRENT_DIR=$(pwd)
echo ""
echo "*** Processing benchmark src directory: " $benchmark_name " ***"
cd benchmarks/${benchmark_name}
bench_dir=$(pwd)

echo "    --> Step 1: Cleaning benchmark directory..."
rm *.s &> /dev/null
rm *.profile &> /dev/null
rm *.pp.bc &> /dev/null
make clean &> /dev/null

echo "---> Step 2: Compiling to bitcode"
make &> err_out
cat err_out | grep "Error"

echo "---> Step 3: Inserting path profiling..."
opt -insert-path-profiling ${benchmark_exec}.bc -o ${benchmark_exec}.pp.bc &> pp_out
llc ${benchmark_exec}.pp.bc -o ${benchmark_exec}.pp.s &>> pp_out
cat pp_out | grep "error"

echo "---> Step 4: Running profiling..."
g++ -o ${benchmark_exec}.profile ${benchmark_exec}.pp.s ${libprofile_rt_path} -lm
eval "$cmd"
echo "-> Execution finished. Output saved to: ${bench_dir}/prog.out"

echo "---> Step 5: Extracting features..."
time opt -load=${path_warmth_module_path} -path-profile-loader -path-profile-loader-file=llvmprof.out -LSTMStaticEstimatorPass ${benchmark_exec}.bc
cat feature.out | grep "error"
echo "-> Feature extraction finished. Output saved to ${bench_dir}/feature.out"


echo "---> Step 6: Done! Moving CSV file..."
mv feature_output.csv $CURRENT_DIR/feature_output_lstm/${benchmark_exec}.csv
echo "-> CSV file saved to ${CURRENT_DIR}/feature_output_lstm/${benchmark_exec}.csv"

#echo "--> Moving back"
cd $CURRENT_DIR

done
