import os, sys, re, os.path
import subprocess, datetime, time, signal
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

total_ops = {}
total_ops["rlu"] = []
total_ops["mvrlu-ordo"] = []
total_ops["vanilla"] = []
#threads = [1,2,4,8,12,16, 20, 24, 32, 40, 48, 56, 64, 128, 192, 224]
threads = [1,2,4,8]
def compile():
    #os.system("make clean > temp.out 2>&1")
    ret = os.system("make -C ../ > temp.out 2>&1")
    if ret != 0:
        print "ERROR in compiling"
        exit(0)

    print "PASS Compile"

def plotgraph(threads):
    fig = plt.figure()
    title = "KyotoDB_Performance"
    ax = fig.add_subplot(111)
    for keys in total_ops:
        ax.plot(threads, total_ops[keys], marker='o', linestyle = '-', label=keys)
    ax.set_ylabel('Operations Per Second')
    ax.set_xlabel('Threads')
    ax.legend(loc = 'upper left')
    fig.savefig(title+'.png')

def write_data_to_file():
    data_file = open("kyoto.dat", "w+")
    for key in total_ops:
        i = 0
        data_file.write("#"+key+"\n")
        for perf in total_ops[key]:
            data_file.write(str(threads[i]) + " " + str(perf) + "\n")
            i = i + 1
    data_file.close()

def run_test(algo, thread):
    cmd = './benchmark-' + algo + ' -t' + str(thread)
    print cmd
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    stdout = process.stdout.readlines()
    PASS = False
    for line in stdout:
        if "PASS" in line:
            PASS = True
        if PASS and "Summary:" in line:
            ops = float(re.search("(?<=total_ops=)[0-9]*", line).group(0))
            total_ops[algo].append(ops)

            return
    print "FAILED execution"
    exit(0)



def run_all():
    compile()
    for thread in threads:
        run_test("rlu", thread)
        run_test("mvrlu-ordo", thread)
        run_test("vanilla", thread)


def run_benchmark():
    run_all()
    plotgraph(threads)
    write_data_to_file()
    #os.system('make clean > temp.out 2>&1')
    os.system('rm -rf temp.out')

if '__main__' == __name__:
    
    run_benchmark()

