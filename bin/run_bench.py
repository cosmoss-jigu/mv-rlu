#!/usr/bin/env python

import json
from run_tests import execute
import optparse
import os
import subprocess
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

def parseresults(log_file, plot_data, t, duration, data_file):
    fp = open(log_file).readlines()
    i = 0
    plot_data[t] = {}
    plot_data[t]['tot_ops'] = []
    plot_data[t]['abrt_ratio'] = []
    data_file.write("#"+t+": thr  ops/us"+"\n")

    for line in fp:
        if i <= 1:
            i += 1
            continue
        w = line.split()
        if not w:
            break
        
        thd = (w[2])
        tot_ops = w[3]
        plot_data[t]['tot_ops'].append(float(tot_ops)/duration/1000)
        abrts = (w[5])
        plot_data[t]['abrt_ratio'].append(float(abrts)/(float(abrts)+float(tot_ops)))
        data_file.write(str(thd) + " " + str(float(tot_ops)/duration/1000) + "\n")
        #print thd
        #print tot_ops
        
def plotgraph(plot_data, threads, update_rate, data_structure, initial_size, graph_type, final_dir):
    fig = plt.figure()
    title = data_structure + '_' + graph_type + '_u' + str(update_rate) + '_i' + str(initial_size)
    fig.suptitle(title)
    ax = fig.add_subplot(111)
    for keys in plot_data:
        ax.plot(threads, plot_data[keys][graph_type], marker='o', linestyle='-', label = keys )
    ax.set_xlabel('threads')
    if graph_type == 'tot_ops':
        ax.set_ylabel('Ops/us')
    else:
        ax.set_ylabel('Abort Ratio')
    ax.legend(loc = 'upper left')
    #plt.show()
    fig.savefig(final_dir+title+'.png')


parser = optparse.OptionParser()
parser.add_option("-d", "--dest", default = "temp",
        help = "destination folder")
parser.add_option("-c", "--config", default = "config.json",
        help = "config file")
parser.add_option("-p", "--plot", default = False,
        help = "plot only")

(opts, args) = parser.parse_args()

#Create result directory
result_dir = "./results/" + opts.dest + "/"
try:
    os.stat(result_dir)
except:
    os.makedirs(result_dir)

#Make benches
#if opts.plot == False:
#    status = subprocess.check_output('make clean -C ..; make -C ..', shell=True)

#Read config files
with open(opts.config) as json_data_file:
    data = json.load(json_data_file)

for test in data:
    if data[test][0]["data_structure"] == "llist":
        if data[test][0]["buckets"] != 1:
            sys.exit("Buckets should be 1\n");
    for ur in data[test][0]["update_rate"]:
        final_dir = result_dir + test + "/u" + str(ur) + "/";

        try:
            os.stat(final_dir)
        except:
            os.makedirs(final_dir)

        plot_data = {}
        data_file = open(final_dir + "plot_data.dat", "w+")
        for t in data[test][0]["alg_type"]:


            out_file = final_dir + "__" + t + "_" +data[test][0]["data_structure"] + "_" + str(data[test][0]["initial_size"]) + "_u" + str(ur) + ".txt"

            if opts.plot == False:
                execute(data[test][0]["runs_per_test"], data[test][0]["rlu_max_ws"], data[test][0]["buckets"], data[test][0]["duration"], \
                    t, ur, data[test][0]["initial_size"], data[test][0]["range_size"], out_file, data[test][0]["zipf_dist_val"], data[test][0]["threads"])

            parseresults(out_file, plot_data, t, \
                    data[test][0]["duration"], data_file)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'tot_ops', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'abrt_ratio', final_dir)
