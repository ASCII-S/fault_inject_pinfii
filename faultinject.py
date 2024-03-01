import sys
import os
import getopt
import time
import random
import signal
import subprocess

pindir = "/root/localTool/pin"
fidir = pindir + "/source/tools/pinfii"
#basedir = "/home/jshwei/Desktop/splash_time_automated"	
#basedir = "."
currdir = "."
pinbin = "pin"
instcategorylib = fidir + "/obj-intel64/instcategory.so"
instcountlib = fidir + "/obj-intel64/instcount.so"
#filib = fidir + "/obj-intel64/duecontinue.so"
filib = fidir + "/obj-intel64/phasefaultinjection.so"
#inputfile = currdir + "/inputs/input.2048"

timeout = 500

optionlist = []

def execute(execlist):
	#print "Begin"
	#inputFile = open(inputfile, "r")
  global outputfile, errordir, outputdir
  print ' '.join(execlist)
  #print outputfile
  outputFile = open(outputfile, "w")
  p = subprocess.Popen(execlist, stdout = outputFile)
  elapsetime = 0
  while (elapsetime < timeout):
    elapsetime += 1
    time.sleep(1)
    # print(p.poll())
    if p.poll() is not None:
      print "\t program finish", p.returncode
      print "\t time taken", elapsetime
      #outputFile = open(outputfile, "w")
      #outputFile.write(p.communicate()[0])
      outputFile.close()
      #inputFile.close()
      return str(p.returncode)
  #inputFile.close()
  outputFile.close()
  print "\tParent : Child timed out. Cleaning up ... "
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
  for index in range(run_number_start, run_number):	#the index of outputfile 
    outputfile = outputdir + "/outputfile-" + str(index)
    errorfile = errordir + "/errorfile-" + str(index)
    execlist = ['mpirun','-np','1',pinbin, '-t', filib,'-o',"./"+progname+"/pin.instcount.txt", '-fi_activation', "./"+progname+"/activate", '-fioption', 'AllInst', "-runindex" ,str(index), '--', progbin]
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

	if progname == "amg":					## amg
		progbin = "/root/localTool/HPL/AMG-master/test/amg"
		optionlist = ['-n','10','10','10'] 	
	elif progname == "FFT":					## FFT
		progbin = "/root/localTool/HPL/splash2/codes/kernels/fft/FFT"
	elif progname == "miniFE":				## miniFE
		progbin = "/root/localTool/HPL/miniFE-master/ref/src/miniFE.x"
		optionlist = ['-nx', '20', '-ny', '30', '-nz', '10']
	elif progname == "backprop":				## backprop
		progbin = "/root/localTool/HPL/rodinia-master/openmp/backprop/backprop"
		optionlist = ['655360']
	elif progname == "Kmeans":				## Kmeans
		progbin = "/root/localTool/HPL/rodinia-master/openmp/kmeans/kmeans"
		datafile = "/root/localTool/HPL/rodinia-master/data/kmeans/inputGen/10000_34.txt"
		optionlist = ['-i', datafile]
	elif progname == "lu":					## LU
		progbin = "/root/localTool/HPL/rodinia-master/openmp/lud/lud"
		optionlist = ['-s512']	
	elif progname == "needle":				## Needle
		progbin = "/root/localTool/HPL/rodinia-master/openmp/nw/needle"
		optionlist = ['2048', '10', '2']
	elif progname == "HPCCG":				## HPCCG
		progbin = "/root/localTool/HPL/HPCCG-master/test_HPCCG"
		optionlist = ['50', '50', '50']
	elif progname == "hpl":					## HPL
		progbin = "/root/localTool/HPL/hpl/bin/test/xhpl"
	elif progname == "XSBench":           			## hang... why?
		progbin = "/root/localTool/HPL/XSBench-master/openmp-threading/XSBench"
	elif progname == "bfs":					## bfs
		progbin = "/root/localTool/HPL/rodinia-master/openmp/bfs/bfs"
		datafile = "/root/localTool/HPL/rodinia-master/data/bfs/graph1M.txt"
		optionlist = [datafile]
	elif progname == "knn":					## KNN
		progbin = "/root/localTool/HPL/rodinia-master/openmp/nn/nn"
		datafile = "./data/nn/list10k.txt"
		optionlist = [datafile, '5', '30', '90']

if __name__=="__main__":
  global run_number_start,run_number, progname
  assert len(sys.argv) == 3 and "Format: prog fi_number"
  progname = sys.argv[1]
  run_number_start = 0
  run_number = int(sys.argv[2])
  set_prog();
  main()

