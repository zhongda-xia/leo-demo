#!/usr/bin/env python
# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

from subprocess import call
from sys import argv
import os
import subprocess
import workerpool
import multiprocessing
import argparse

import datetime

######################################################################
######################################################################
######################################################################

parser = argparse.ArgumentParser(description='Simulation runner')
parser.add_argument('scenarios', metavar='scenario', type=str, nargs='*',
                    help='Scenario to run')

parser.add_argument('-l', '--list', dest="list", action='store_true', default=False,
                    help='Get list of available scenarios')

parser.add_argument('-s', '--simulate', dest="simulate", action='store_true', default=False,
                    help='Run simulation and postprocessing (false by default)')

parser.add_argument('-g', '--no-graph', dest="graph", action='store_false', default=True,
                    help='Do not build a graph for the scenario (builds a graph by default)')

args = parser.parse_args()

if not args.list and len(args.scenarios)==0:
    print("ERROR: at least one scenario need to be specified")
    parser.print_help()
    exit (1)

if args.list:
    print("Available scenarios: ")
else:
    if args.simulate:
        print("Simulating the following scenarios: " + ",".join (args.scenarios))

    if args.graph:
        print("Building graphs for the following scenarios: " + ",".join (args.scenarios))

######################################################################
######################################################################
######################################################################

class SimulationJob (workerpool.Job):
    "Job to simulate things"
    def __init__ (self, cmdline):
        self.cmdline = cmdline
    def run (self):
        print (" ".join (self.cmdline))
        retcode = subprocess.call (self.cmdline)
        print ("result:", " ".join (self.cmdline), retcode)

pool = workerpool.WorkerPool(size = multiprocessing.cpu_count())

class Processor:
    def run (self):
        if args.list:
            print ("    " + self.name)
            return

        if "all" not in args.scenarios and self.name not in args.scenarios:
            return

        if args.list:
            pass
        else:
            if args.simulate:
                self.simulate ()
                pool.join ()
                self.postprocess ()
            if args.graph:
                self.graph ()

    def graph (self):
        subprocess.call ("./graphs/%s.R" % self.name, shell=True)

class ConsumerScenario (Processor):
    def __init__ (self, name, cmdLine, dataDir, doShim = False):
        self.name = name
        self.cmdLine = cmdLine
        # other initialization, if any
        self.dataDir = dataDir
        self.doShim = doShim

    def exhaust(self, params, cmdline, prefix, pos=0):
        if pos == len(params.keys()):
            cmdline += ["--resPrefix=" + prefix]
            cmdline += ["--dataDir=" + self.dataDir]
            job = SimulationJob (cmdline)
            pool.put (job)
            return
        paramKeyList = list(params.keys())
        for param in params[paramKeyList[pos]]:
            self.exhaust(params, 
                         cmdline + ["--" + paramKeyList[pos] + "=" + str(param)], 
                         prefix + paramKeyList[pos] + "_" + str(param) + "-",
                         pos + 1)

    def simulate (self):
        cmdline = [self.cmdLine]
        params = dict()

        params["run"] = [1]
        params["stop"] = [95]

        params["consumerCbrFreq"] = ["50.0", "100.0", "150.0", "200.0", "250.0", "300.0"]

        params["consumerCity"] = [s.replace(' ', '+') for s in ["Beijing"]]
        params["producerCity"] = [s.replace(' ', '+') for s in ["Chicago"]]

        if not self.doShim:
            params["strategy"] = ["multicast", "retx", "hint"]
            params["doShim"] = ["false"]
            params["hopLimit"] = [0]
        else:
            params["strategy"] = ["multicast"]
            params["doShim"] = ["true"]
            params["hopLimit"] = [2, 3]

        TIMEFORMAT = '%Y-%m-%d_%H:%M:%S'
        sysTime = datetime.datetime.now().strftime(TIMEFORMAT)
        path = "results/" + sysTime + "/"
        os.makedirs(path)
        self.exhaust(params, cmdline, path)

    def postprocess (self):
        # any postprocessing, if any
        pass


try:
    # Simulation, processing, and graph building
    consumerCmdLine = r"./build/sat-p2p"
    producerCmdLine = r"./build/sat-p2p-kite"
    dataDir = r"../../ndnsim_files"

    consumerScenario = ConsumerScenario (name="leo-consumer-strategy", cmdLine=consumerCmdLine, dataDir=dataDir)
    consumerScenario.run ()

    consumerShimScenario0 = ConsumerScenario (name="leo-consumer-shim", cmdLine=consumerCmdLine, dataDir=dataDir, doShim=True)
    consumerShimScenario0.run ()
    consumerShimScenario1 = ConsumerScenario (name="leo-consumer-shim", cmdLine=consumerCmdLine, dataDir=dataDir, doShim=False)
    consumerShimScenario1.run ()

finally:
    pool.join ()
    pool.shutdown ()
