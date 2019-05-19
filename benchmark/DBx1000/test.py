import os, sys, re, os.path
import platform
import subprocess, datetime, time, signal
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
dbms_cfg = ["config-std.h", "config.h"]
#algs = ['DL_DETECT', 'NO_WAIT', 'HEKATON', 'SILO', 'TICTOC']
algs = ['MVRLU', 'TICTOC', 'SILO', 'HEKATON']
ts = ['TS_CAS']

def replace(filename, pattern, replacement):
	f = open(filename)
	s = f.read()
	f.close()
	s = re.sub(pattern,replacement,s)
	f = open(filename,'w')
	f.write(s)
	f.close()

def insert_job(jobs, alg, workload, thread, t): 
        insert_job.counter += 1
        if alg == 'MVRLU':
            t = 'TS_HW'
        
	jobs[insert_job.counter] = {
		"WORKLOAD"			: workload,
		"THREAD_CNT"			: thread,
		"CC_ALG"			: alg,
                "TS_ALLOC"                      : t,
	}

def test_compile(job):
	os.system("cp "+ dbms_cfg[0] +' ' + dbms_cfg[1])
	for (param, value) in job.iteritems():
		pattern = r"\#define\s*" + re.escape(param) + r'.*'
		replacement = "#define " + param + ' ' + str(value)
		replace(dbms_cfg[1], pattern, replacement)
	os.system("make clean > temp.out 2>&1")
	ret = os.system("make -j > temp.out 2>&1")
	if ret != 0:
		print "ERROR in compiling job="
		print job
		exit(0)
	print "PASS Compile\t\talg=%s,\tworkload=%s\tthreads=%d" % (job['CC_ALG'], job['WORKLOAD'],job["THREAD_CNT"])

def test_run(test, job, abort_rate, perf):
	app_flags = ""
	if test == 'read_write':
		app_flags = "-Ar -t1"
	if test == 'conflict':
		app_flags = "-Ac -t4"
	
	#os.system("./rundb %s > temp.out 2>&1" % app_flags)
	#cmd = "./rundb %s > temp.out 2>&1" % app_flags
	cmd = "./rundb %s" % (app_flags)
	start = datetime.datetime.now()
	process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
        timeout = 10000 # in second
	while process.poll() is None:
		time.sleep(1)
		now = datetime.datetime.now()
		if (now - start).seconds > timeout:
			os.kill(process.pid, signal.SIGKILL)
			os.waitpid(-1, os.WNOHANG)
			print "ERROR. Timeout cmd=%s" % cmd
			exit(0)
        
        stdout = process.stdout.readlines()
        stderr = process.stderr.read()
        print stderr
        #print stdout
        PASS = False
        for line in stdout:
	    if "PASS" in line:
		    if test != '':
		    	    print "PASS execution. \talg=%s,\tworkload=%s(%s)\tthread=%d" % \
				    (job["CC_ALG"], job["WORKLOAD"], test, job["THREAD_CNT"])
	    	    else :
			    print "PASS execution. \talg=%s,\tworkload=%s\tthreads=%d" % \
				    (job["CC_ALG"], job["WORKLOAD"],job["THREAD_CNT"])
                    PASS = True
            if PASS and "summary" in line:
                    abrt_cnt = int(re.search("(?<=abort_cnt=)[0-9]*", line).group(0))
                    run_time = float(re.search("(?<=run_time=)[0-9]*\.[0-9]*", line).group(0))
                    txn_cnt = float(re.search("(?<=txn_cnt=)[0-9]*", line).group(0))
                    #print abrt_cnt, run_time, txn_cnt
                    abrt_rate = abrt_cnt / (txn_cnt+abrt_cnt)
                    tps = txn_cnt * job['THREAD_CNT'] / run_time / 1000
                    abort_rate[job['CC_ALG']+'_'+job['TS_ALLOC']].append(abrt_rate)
                    perf[job['CC_ALG']+'_'+job['TS_ALLOC']].append(int(tps))
                    return

	print "FAILED execution. cmd = %s" % cmd
	exit(0)

def run_all_test(jobs, abort_rate, perf) :
	for (jobname, job) in jobs.iteritems():
		test_compile(job)
		if job['WORKLOAD'] == 'TEST':
			test_run('read_write', job, abort_rate, perf)
			#test_run('conflict', job)
		else :
			test_run('', job, abort_rate, perf)
	jobs = {}

def plotgraph(plot_data, threads, benchmark_name, operation):
    fig = plt.figure()
    title = benchmark_name+'_'+operation
    fig.suptitle(title)
    ax = fig.add_subplot(111)
    for keys in plot_data:
        ax.plot(threads, plot_data[keys], marker='o', linestyle='-', label=keys)
    ax.set_xlabel('threads')
    if operation == 'abort_rate':
        ax.set_ylabel('abort_rate')
    else:
        ax.set_ylabel('Ops(in thousands)')
    ax.legend(loc='upper left')
    fig.savefig(title+'.png')

def write_data_to_file(plot_data):
    data_file = open("dbx1000.dat", "w+")
    for key in plot_data:
        i = 0
        data_file.write("#"+key+"\n")
        for perf in plot_data[key]:
            data_file.write(str(threads[i]) + " " + str(perf) + "\n")
            i = i + 1
    data_file.close()

def run_benchmarks(algs, threads):
    # run YCSB tests
    insert_job.counter=0
    jobs = {}
    ycsb_abort_rate = {}
    ycsb_perf = {}
    tpcc_abort_rate = {}
    tpcc_perf = {}

    for alg in algs: 
        for t in ts:
                if alg == 'MVRLU':
                    t = 'TS_HW'
                ycsb_abort_rate[alg+'_'+t] = []
                ycsb_perf[alg+'_'+t] = []
                for thread in threads:
                    insert_job(jobs, alg, 'YCSB', thread, t)
    run_all_test(jobs, ycsb_abort_rate, ycsb_perf)
    write_data_to_file(ycsb_perf)
    plotgraph(ycsb_perf, threads, 'ycsb', 'perf')
    plotgraph(ycsb_abort_rate, threads, 'ycsb', 'abort_rate')

    #run TPCC tests
    insert_job.counter=0
    #jobs = {}
    #for alg in algs: 
    #    for t in ts:
    #        tpcc_abort_rate[alg+'_'+t] = []
    #        tpcc_perf[alg+'_'+t] = []
    #        for thread in threads:
    #            insert_job(jobs, alg, 'TPCC', thread, t)
    #run_all_test(jobs, tpcc_abort_rate, tpcc_perf)
    #plotgraph(tpcc_perf, threads, 'tpcc', 'perf')
    #plotgraph(tpcc_abort_rate, threads, 'tpcc', 'abort_rate')


    os.system('cp config-std.h config.h')
    os.system('make clean > temp.out 2>&1')
    os.system('rm temp.out')

if '__main__' == __name__:
    threads = [1, 2, 4, 8, 16, 32, 60, 96, 120, 128, 160, 192]
    #threads = [1, 2]
    run_benchmarks(algs, threads)
