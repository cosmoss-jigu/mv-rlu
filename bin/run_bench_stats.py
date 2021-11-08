#!/usr/bin/env python

# SPDX-FileCopyrightText: Copyright (c) 2018-2019 Virginia Tech
# SPDX-License-Identifier: Apache-2.0

import json
from run_tests_stats import execute
import optparse
import os
import subprocess
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt

def parseresults(log_file, plot_data, t, duration):
    fp = open(log_file).readlines()
    i = 0
    plot_data[t] = {}
    plot_data[t]['tot_ops'] = []
    plot_data[t]['abrt_ratio'] = []
    plot_data[t]['ckp_builder_start'] = []
    plot_data[t]['writeback_spent'] = []
    plot_data[t]['blocking_spent'] = []
    plot_data[t]['ckp_quiescence_spent'] = []
    plot_data[t]['ckp_scan_spent'] = []
    plot_data[t]['ckp_builder_spent'] = []
    plot_data[t]['ckp_barrier_spent'] = []
    plot_data[t]['max_last_objs'] = []
    plot_data[t]['avg_last_objs'] = []
    plot_data[t]['ckp_builder_by_wakeup'] = []
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
        ckp_builder_start = (w[6])
        writeback_spent = (w[7])
        blocking_spent = (w[8])
        ckp_quiescence_spent = (w[9])
        ckp_scan_spent = (w[10])
        ckp_builder_spent = (w[11])
        ckp_barrier_spent = (w[12])
        max_last_objs = (w[13])
        avg_last_objs = (w[14])
        ckp_builder_by_wakeup = (w[15])
        plot_data[t]['ckp_builder_start'].append(ckp_builder_start)
        plot_data[t]['writeback_spent'].append(writeback_spent)
        plot_data[t]['blocking_spent'].append(blocking_spent)
        plot_data[t]['ckp_quiescence_spent'].append(ckp_quiescence_spent)
        plot_data[t]['ckp_scan_spent'].append(ckp_scan_spent)
        plot_data[t]['ckp_builder_spent'].append(ckp_builder_spent)
        plot_data[t]['ckp_barrier_spent'].append(ckp_barrier_spent)
        plot_data[t]['max_last_objs'].append(max_last_objs)
        plot_data[t]['avg_last_objs'].append(avg_last_objs)
        plot_data[t]['ckp_builder_by_wakeup'].append(ckp_builder_by_wakeup)
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

(opts, args) = parser.parse_args()

#Create result directory
result_dir = "./results/" + opts.dest + "/"
try:
    os.stat(result_dir)
except:
    os.makedirs(result_dir)

#Make benches
status = subprocess.check_output('make clean -C ../src/; make -C ../src/', shell=True)

#Read config files
with open('config.json') as json_data_file:
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
        for t in data[test][0]["alg_type"]:
            out_file = final_dir + "__" + t + "_" +data[test][0]["data_structure"] + "_" + str(data[test][0]["initial_size"]) + "_u" + str(ur) + ".txt"

            execute(data[test][0]["runs_per_test"], data[test][0]["rlu_max_ws"], data[test][0]["buckets"], data[test][0]["duration"], \
                    t, ur, data[test][0]["initial_size"], data[test][0]["range_size"], out_file, data[test][0]["threads"])

            parseresults(out_file, plot_data, t, data[test][0]["duration"])
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'tot_ops', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'abrt_ratio', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'ckp_builder_start', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'writeback_spent', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'blocking_spent', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'ckp_quiescence_spent', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'ckp_scan_spent', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'ckp_builder_spent', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'ckp_barrier_spent', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'max_last_objs', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'avg_last_objs', final_dir)
        plotgraph(plot_data, data[test][0]["threads"], ur, data[test][0]["data_structure"], data[test][0]["initial_size"], 'ckp_builder_by_wakeup', final_dir)
