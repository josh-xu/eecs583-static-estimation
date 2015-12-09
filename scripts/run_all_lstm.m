clc; clear all;

% get platform
isOctave = exist('OCTAVE_VERSION', 'builtin') ~= 0

% open file
fileID = fopen('benchmark_list_poly.csv');

% load data
data = textscan(fileID, '%s %s %s','delimiter',',');

% get number of rows
numRows = length(data{1});

% octave fix
if isOctave
	data{3}(41) = ' ';
end

% for each row
for i=1:numRows
	% show progress
	fprintf('%d/%d\n', i, numRows)

	% get the three variables
	benchmark_name = data{1}{i};
	benchmark_exec = data{2}{i};
	benchmark_load = data{3}{i};

	% call run.sh with these strings
	cmd = sprintf('./run_lstm.sh "%s" "%s" "%s"',benchmark_name, benchmark_exec, benchmark_load);

	% run shell script with input
	system(cmd);	
end
