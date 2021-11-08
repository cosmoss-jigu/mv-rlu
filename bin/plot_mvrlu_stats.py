# SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
# SPDX-License-Identifier: Apache-2.0

import time
import sys
import os, subprocess
import optparse
from threading import Timer
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

IS_NUMA = 0
IS_2_SOCKET = 0
IS_PERF = 0

CMD_PARAMS = '-b%d -d%d -u%d -i%d -r%d -n%d'

PERF_FILE = "__perf_output.file"
CMD_PREFIX_PERF = "perf stat -d -o %s" % (PERF_FILE,)

CMD_BASE_MVRLU = './bench-mvrlu-gclk'
CMD_BASE_MVRLU_ORDO = './bench-mvrlu-ordo'

OUTPUT_FILENAME = '___temp.file'

W_OUTPUT_FILENAME = '__w_check.txt'

CMD_PREFIX_LIBS = 'export LD_PRELOAD=\\"/usr/lib64/libtcmalloc_minimal.so.4\\"'
#CMD_PREFIX_LIBS = 'export LD_PRELOAD=\\"$LD_PRELOAD\\"'

LLIST_SCRIPT = 'plot_scripts/llist'
HLIST_SCRIPT = 'plot_scripts/hlist'

CMD_NUMA_BIND_TO_CPU_0 = 'numactl --cpunodebind=0 '
CMD_NUMA_BIND_TO_CPU_1 = 'numactl --cpunodebind=1 '
CMD_NUMA_BIND_TO_CPU_0_1 = 'numactl --cpunodebind=0,1 '
CMD_NUMA_PREFIX_8  = 'taskset -c 0-7 '
CMD_NUMA_PREFIX_10 = 'taskset -c 0-9 '
CMD_NUMA_PREFIX_12 = 'taskset -c 0-11 '
CMD_NUMA_PREFIX_14 = 'taskset -c 0-13 '
CMD_NUMA_PREFIX_15 = 'taskset -c 0-14 '
CMD_NUMA_PREFIX_16 = 'taskset -c 0-15 '
CMD_NUMA_PREFIX_30 = 'taskset -c 0-29 '
CMD_NUMA_PREFIX_45 = 'taskset -c 0-44 '
CMD_NUMA_PREFIX_60 = 'taskset -c 0-59 '
CMD_NUMA_PREFIX_75 = 'taskset -c 0-74 '
CMD_NUMA_PREFIX_90 = 'taskset -c 0-89 '
CMD_NUMA_PREFIX_105= 'taskset -c 0-104 '
CMD_NUMA_PREFIX_120= 'taskset -c 0-119 '

CMD_BASE = {
	'mvrlu' : CMD_BASE_MVRLU,
        'mvrlu_ordo' : CMD_BASE_MVRLU_ORDO,
}

result_keys = [
'                        n_starts =',#0
'                        n_finish =',#1
'                        n_aborts =',#2
'               n_low_mark_wakeup =',#3
'               n_high_mark_block =',#4
'              max_log_used_bytes =',#5
'                       n_reclaim =',#6
'               n_reclaim_wrt_set =',#7
'                  n_reclaim_copy =',#8
'                  n_reclaim_free =',#9
'                     n_qp_detect =',#10
'                        n_qp_nap =',#11
'               n_qp_help_reclaim =' #12
]

perf_result_keys = [
	'instructions              #',
	'branches                  #',
	'branch-misses             #',
	'L1-dcache-loads           #',
	'L1-dcache-load-misses     #',
]
benchmark_keys = [
        'low_watermark',
        'high_watermark',
        'abrt_ratio',
        'reclaim',
        'help_reclaim'
        ]
def parseresults(log_file, plot_data):
    print log_file
    fp = open(log_file).readlines()
    i = 0
    for key in benchmark_keys:
        plot_data[key] = []
    for line in fp:
        if i <= 1:
            i += 1
            continue
        w = line.split()
        if not w:
            break
        tot_ops = w[0]
        abrts = (w[2])
        plot_data['abrt_ratio'].append(float(abrts)/(float(abrts)+float(tot_ops)))
        plot_data['low_watermark'].append(w[3])
        plot_data['high_watermark'].append(w[4])
        plot_data['reclaim'].append(w[6])
        plot_data['help_reclaim'].append(w[12])
        
def plotgraph(plot_data, threads, update_rate, data_structure, initial_size, graph_type, final_dir):
    fig = plt.figure()
    title = data_structure + '_' + graph_type + '_u' + str(update_rate) + '_i' + str(initial_size)
    fig.suptitle(title)
    ax = fig.add_subplot(111)
    ax.plot(threads, plot_data[graph_type], marker='o', linestyle='-' )
    ax.set_xlabel('threads')
    ax.set_ylabel(graph_type)
    #plt.show()
    fig.savefig(final_dir+title+'.png')

def cmd_numa_prefix(threads_num):
	if (IS_2_SOCKET):
		if (threads_num <= 36):
	                print 'cmd_numa_prefix: BIND_CPU th_num = %d' % (threads_num,)
			return CMD_NUMA_BIND_TO_CPU_1

		return CMD_NUMA_BIND_TO_CPU_0_1

        print 'cmd_numa_prefix: th_num = %d' % (threads_num)

	if (threads_num <= 15):
		return CMD_NUMA_PREFIX_15

	if (threads_num <= 30):
		return CMD_NUMA_PREFIX_30

	if (threads_num <= 45):
		return CMD_NUMA_PREFIX_45

	if (threads_num <= 60):
		return CMD_NUMA_PREFIX_60

	if (threads_num <= 75):
		return CMD_NUMA_PREFIX_75

        if (threads_num <= 90):
		return CMD_NUMA_PREFIX_90

        if (threads_num <= 105):
		return CMD_NUMA_PREFIX_105

        if (threads_num <= 120):
		return CMD_NUMA_PREFIX_120

	print 'cmd_numa_prefix: ERROR th_num = %d' % (threads_num,)


def extract_data(output_data, key_str):
	data = output_data.split(key_str)[1].split()[0].strip()

	if (data.find('nan') != -1):
		return 0

	if (key_str == 'L1-dcache-load-misses     #') or (key_str == 'branch-misses             #'):
		data = data.strip('%')

	return float(data)


def extract_keys(output_data):
	d = {}

	for key in result_keys:
		d[key] = extract_data(output_data, key)

	if IS_PERF:
		for key in perf_result_keys:
			d[key] = extract_data(output_data, key)

	return d


def print_keys(dict_keys):
	print '================================='
	for key in result_keys:
		print '%s %.2f' % (key, dict_keys[key])

	if IS_PERF:
		for key in perf_result_keys:
			print '%s %.2f' % (key, dict_keys[key])


def run_test(runs_per_test, alg_type, cmd):

	ops_total = 0
	total_operations = 0
	aborts_total = 0
	total_combiners = 0
	total_num_of_waiting = 0
	total_additional_readers = 0
	total_additional_writers = 0

	cmd_prefix = 'timeout 300 bash -c "' + CMD_PREFIX_LIBS + ' ;'
	if (IS_PERF):
		cmd_prefix += CMD_PREFIX_PERF + ' '

	full_cmd = cmd_prefix + cmd + ' && echo $? > done"'

	print full_cmd

	total_dict_keys = {}
	for key in result_keys:
		total_dict_keys[key] = 0

        i = 0
	while True:
		print 'run %d ' % (i,)


		if (IS_PERF):
			try:
				os.unlink(PERF_FILE)
			except OSError:
				pass

                os.system("rm -rf done")
		os.system('w >> %s' % (W_OUTPUT_FILENAME,))
		time.sleep(1)
                os.system(full_cmd + '>' +  OUTPUT_FILENAME)
		os.system('w >> %s' % (W_OUTPUT_FILENAME,))

                if os.path.exists("./done") == False:
                    continue
                done = open("done", 'rb')
                rv = done.read()
                print rv
                if rv == '-11':
                    done.close()
                    os.system("rm -rf done")
                    time.sleep(5)
                    continue
                done.close()
                os.system("rm -rf done")


		time.sleep(1)
		f = open(OUTPUT_FILENAME, 'rb')
		output_data = f.read()
		f.close()
	        os.unlink(OUTPUT_FILENAME)

		if (IS_PERF):
			f = open(PERF_FILE, 'rb');
			output_data += f.read()
			f.close()
			os.unlink(PERF_FILE)

		print "------------------------------------"
		print output_data
		print "------------------------------------"

		dict_keys = extract_keys(output_data)

		print '================================='
		for key in dict_keys.keys():
			total_dict_keys[key] += dict_keys[key]

		print_keys(dict_keys)
                i = i + 1
                if i == runs_per_test:
                    break


	for key in total_dict_keys.keys():
		total_dict_keys[key] /= runs_per_test

	return total_dict_keys

def print_run_results(f_out, rlu_max_ws, update_ratio, th_num, dict_keys):

        f_out.write('\n')
	for key in result_keys:
		f_out.write(' %.2f' % dict_keys[key])

	if IS_PERF:
		for key in perf_result_keys:
			f_out.write(' %.2f' % dict_keys[key])

	f_out.flush()


def execute(runs_per_test,
                        rlu_max_ws,
			buckets,
			duration,
			alg_type,
			update_ratio,
			initial_size,
			range_size,
			output_filename,
			th_num_list):


	f_w = open(W_OUTPUT_FILENAME, 'wb');
	f_w.close()

	f_out = open(output_filename, 'wb')

	cmd_header = '[%s] ' % (alg_type,) + CMD_BASE[alg_type] + ' ' + CMD_PARAMS % (
		buckets,
		duration,
		update_ratio,
		initial_size,
		range_size,
		0)

	f_out.write(cmd_header + '\n')
	f_out.flush()

	results = []
	for th_num in th_num_list:


		cmd = CMD_BASE[alg_type] + ' ' + CMD_PARAMS % (
			buckets,
			duration,
			update_ratio,
			initial_size,
			range_size,
			th_num)


		print '-------------------------------'
		print '[%d] %s ' % (th_num, cmd)
                if(IS_NUMA):
                    cmd = cmd_numa_prefix(th_num) + cmd

		dict_keys = run_test(runs_per_test, alg_type, cmd)

		results.append(dict_keys)

		print_run_results(f_out, rlu_max_ws, update_ratio, th_num, dict_keys)


	f_out.write('\n\n')
	f_out.flush()
	f_out.close()
        del result_keys[-1]


	print 'DONE: written output to %s' % (output_filename,)


if '__main__' == __name__:


    parser = optparse.OptionParser()
    parser.add_option("-k", "--runs", type = 'int', default = 1,
                    help = "runs per test")
    parser.add_option("-w", "--rlu-max-ws", type = 'int', default = 1,
                    help = "Maximum rlu write set aggregated in RLU defferal")
    parser.add_option("-b", "--buckets", type = 'int', default = 1,
                    help = "Number of buckets")
    parser.add_option("-d", "--duration", type = 'int', default = 10000,
                    help = "Duration of run in ms")
    parser.add_option("-t", "--alg-type", default = "mvrlu_ordo",
                    help = "type of algorithm")
    parser.add_option("-u", "--update-rate", type = 'int', default = 0,
                    help = "Percentage of update transaction: 0-1000(100%)")
    parser.add_option("-i", "--initial_size", type = 'int', default =1000,
                    help = "Number of elements to insert before test")
    parser.add_option("-r", "--range_size", type = 'int', default = 2000,
                    help = "range size")
    parser.add_option("-n", "--th_num_list", type = 'int', action = "append",
                    dest = 'th_num_list', 
                    default = [1, 4, 8, 14, 28, 56, 84, 112, 140, 168, 196,
                        224], 
                    help = "Number of threads")
    parser.add_option("-f", "--output-filename", default = "__result.txt",
                    help = "Output Filename")
    parser.add_option("-g", "--generate-graph", type = 'int', default = 0,
                    help = "Generate graph")


    (opts, args) = parser.parse_args()

    #check options
    for opt in vars(opts):
        val = getattr(opts, opt)
        if val == None:
            print("Missing options: %s" %opt)
            parser.print_help()
            exit(1)
        if len(opts.th_num_list) == 0:
            print("Number of threads required\n")
            parser.print_help()
            exit(1)

    result_dir = "./results/"
    llist = False

    if opts.buckets == 1:
        result_dir = result_dir + "llist/" + "u" + str(opts.update_rate) + "/"
        llist = True
    else:
        result_dir = result_dir + "hlist/" + "u" + str(opts.update_rate) + "/"


    try:
        os.stat(result_dir)
    except:
        os.makedirs(result_dir)
    output_filename = result_dir + opts.output_filename
    execute(opts.runs, opts.rlu_max_ws, opts.buckets, opts.duration,
            opts.alg_type, opts.update_rate, opts.initial_size, opts.range_size,
            output_filename, opts.th_num_list)
    plot_data = {}

    parseresults(output_filename, plot_data)
    for key in benchmark_keys:
        plotgraph(plot_data, opts.th_num_list, opts.update_rate, 'mvrlu-ordo',
            opts.initial_size, key, result_dir)
