import pandas as pd
import subprocess

poly = pd.read_csv('benchmark_list_ptr.csv', header=None)
files = poly.iloc[:,1]
csv_files = ['features_files_lstm_all/{}.csv'.format(c.strip()) for c in files]
for f in csv_files:
    cmd = 'cat {} >> features_files_lstm/ptrbench.csv'.format(f)
    print(cmd)
    subprocess.call(cmd, shell=True)
print('done')
