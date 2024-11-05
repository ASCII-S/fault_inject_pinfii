#!/usr/bin/python

import sys
import os
import getopt
import time
import random
import signal
import subprocess
pindir = "/home/tongshiyu/pin"
fidir = pindir + "/source/tools/pinfi"
#basedir = "/home/jshwei/Desktop/splash_time_automated"
#basedir = "."
currdir = "."
pinbin = "pin"
instcategorylib = fidir + "/obj-intel64/instcategory.so"
instcountlib = fidir + "/obj-intel64/instcount.so"
#filib = fidir + "/obj-intel64/duecontinue.so"
filib = fidir + "/obj-intel64/faultinjection.so"
#inputfile = currdir + "/inputs/input.2048"
outputdir = currdir + "/prog_output"
basedir = currdir + "/baseline"
errordir = currdir + "/error_output"

timeout = 500

optionlist = []
progbin ="/home/tongshiyu/programs/LLNL/AMG/test/amg"

def execute(execlist):
        #print "Begin"
        #inputFile = open(inputfile, "r")
  global outputfile, errordir, outputdir
  print (' '.join(execlist))
  #print outputfile
  outputFile = open(outputfile, "w")
  p = subprocess.Popen(execlist, stdout = outputFile)
  elapsetime = 0
  while (elapsetime < timeout):
    elapsetime += 1
    time.sleep(1)
    # print(p.poll())
    if p.poll() is not None:
      print ("\t program finish", p.returncode)
      print ("\t time taken", elapsetime)
      #outputFile = open(outputfile, "w")
      #outputFile.write(p.communicate()[0])
      outputFile.close()
      #inputFile.close()
      return str(p.returncode)
  #inputFile.close()
  outputFile.close()
  print ("\tParent : Child timed out. Cleaning up ... ")
  p.kill()
  return "timed-out"
        #should never go here
  sys.exit(syscode)


def main():
  #clear previous output
  global run_number_start,run_number, optionlist, outputfile, progbin, progname
  outputfile = basedir + "/golden_output"
  execlist = ['mpirun','-np','1',pinbin, '-t', instcategorylib,'-o',"./"+progname+"/pin.instcategory.txt", '--', progbin]
  execlist.extend(optionlist)
  execute(execlist)


  # baseline
  outputfile = basedir + "/golden_output"
  execlist = ['mpirun','-np','1',pinbin, '-t', instcountlib, '-o',"./"+progname+"/pin.instcount.txt",'--', progbin]
  execlist.extend(optionlist)
  execute(execlist)

  # fault injection
  for index in range(run_number_start, run_number):     #the index of outputfile 
    outputfile = outputdir + "/outputfile-" + str(index)
    errorfile = errordir + "/errorfile-" + str(index)
    execlist = ['mpirun','-np','1',pinbin, '-t', filib,'-o',"./"+progname+"/pin.instcount.txt", '-fi_activation', "./"+progname+"/activate", '-fioption', 'AllInst', "-index" ,str(index), '--', progbin]
    execlist.extend(optionlist)
    ret = execute(execlist)
    if ret == "timed-out":
      error_File = open(errorfile, 'w')
      error_File.write("Program hang\n")
      error_File.close()
    elif int(ret) < 0:
      error_File = open(errorfile, 'w')
      error_File.write("Program crashed, terminated by the system, return code " + ret + '\n')
      error_File.close()
    elif int(ret) > 0:
      error_File = open(errorfile, 'w')
      error_File.write("Program crashed, terminated by itself, return code " + ret + '\n')
      error_File.close()


def set_prog():
        global progname, optionlist, outputfile, basedir, errordir, outputdir, progbin
        basedir = currdir + "/" + progname + "/baseline"
        errordir = currdir + "/" + progname + "/error_output"
        outputdir = currdir + "/" + progname + "/prog_output"
        if not os.path.isdir(outputdir):
                os.mkdir(outputdir)
        if not os.path.isdir(basedir):
                os.mkdir(basedir)
        if not os.path.isdir(errordir):
                os.mkdir(errordir)

        if progname == "amg":                                   ## amg
                progbin = "/home/tongshiyu/programs/LLNL/AMG/test/amg"
                optionlist = ['-n','5','5','5']         
        elif progname == "FFT":                                 ## FFT
                progbin = "/root/localTool/HPL/splash2/codes/kernels/fft/FFT"
        elif progname == "miniFE":                              ## miniFE
                progbin = "/root/localTool/HPL/miniFE-master/ref/src/miniFE.x"
                optionlist = ['-nx', '20', '-ny', '30', '-nz', '10']
        elif progname == "backprop":                            ## backprop
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/backprop/backprop"
                optionlist = ['65536']
        elif progname == "Kmeans":                              ## Kmeans
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/kmeans/kmeans"
                datafile = "/home/tongshiyu/programs/rodinia-master/data/kmeans/inputGen/1000_34.txt"
                optionlist = ['-i', datafile]
        elif progname == "lu":                                  ## LU
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/lud/lud"
                optionlist = ['-s512']  
        elif progname == "needle":                              ## Needle
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/nw/needle"
                optionlist = ['2048', '10', '2']
        elif progname == "HPCCG":                               ## HPCCG
                progbin = "/root/localTool/HPL/HPCCG-master/test_HPCCG"
                optionlist = ['50', '50', '50']
        elif progname == "hpl":                                 ## HPL
                progbin = "/home/tongshiyu/programs/hpl-2.3/testing/xhpl"
        elif progname == "bfs":                                 ## bfs
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/bfs/bfs"
                datafile = "/home/tongshiyu/programs/rodinia-master/data/bfs/inputGen/graph64k.txt"
                optionlist = [datafile]
        elif progname == "knn":                                 ## KNN
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/nn/nn"
                datafile = "./filelist.txt"
                optionlist = [datafile, '5', '30', '90']
        elif progname == "myocyte":                             
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/myocyte/myocyte"
                optionlist = ['1000', '1', '0', '4']
        elif progname == "srad":
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/srad_v1/srad"
                optionlist = ['15', '0.5', '285', '250', '1']
        elif progname == "lavaMD":
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/lavaMD/lavaMD"
                optionlist = ['-boxes1d', '5']
        elif progname == "heartwall":
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/heartwall/heartwall"
                optionlist = ["/home/tongshiyu/programs/rodinia-master/data/heartwall/test.avi.part00",'20']
        elif progname == "pathfinder":
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/pathfinder/pathfinder"
                optionlist = ["1000",'10']
        elif progname == "b+tree":
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/b+tree/b+tree"
                optionlist = ['file', '/home/tongshiyu/programs/rodinia-master/data/b+tree/mil.txt', 'command', '/home/tongshiyu/programs/rodinia-master/data/b+tree/command.txt']
        elif progname == "hotspot":
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/hotspot/hotspot"
                optionlist = ['64','64','2','1',"/home/tongshiyu/programs/rodinia-master/data/hotspot/temp_64",'/home/tongshiyu/programs/rodinia-master/data/hotspot/power_64', './hotspot/outfile']
        elif progname == "leukocyte":
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/leukocyte/leukocyte"
                optionlist = ['5', '4', "/home/tongshiyu/programs/rodinia-master/data/leukocyte/testfile.avi"]
        elif progname == "particlefilter":
                progbin = "/home/tongshiyu/programs/rodinia-master/openmp/particlefilter/particle_filter"
                optionlist = ['-x', '64', '-y', '64', '-z', '10', '-np', '1000']
        elif progname == "miniMD":
                progbin = "/home/tongshiyu/programs/mantevo/miniMD/ref/miniMD_openmpi"
        elif progname == "miniAMR":
                progbin = "/home/tongshiyu/programs/mantevo/miniAMR/ref/miniAMR.x"
                optionlist = ['--num_refine', '4', '--max_blocks', '2000', '--nx', '2', '--ny', '2', '--nz', '2']
        elif progname == "XSBench":
                progbin = "/home/tongshiyu/programs/ANL/XSBench/openmp-threading/XSBench"
                optionlist = ['-s', 'small', '-p', '100']

if __name__=="__main__":
  global run_number_start,run_number, progname
  assert len(sys.argv) == 3 and "Format: prog fi_number"
  progname = sys.argv[1]
  run_number_start = 0
  run_number = int(sys.argv[2])
  set_prog();
  main()

