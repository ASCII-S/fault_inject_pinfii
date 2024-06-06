import os
import io
import sys
import linecache
import re
from pandas.core.frame import DataFrame

# arguments of injection
segments = 50
fiPerSeg = 300
fiTotal = 20000
# split masked between crash by comparing indexs in folder errorFile and progFile
errorFileIndexes=[]
# store list in dictionary so that to_csv can make table 
df_per_fi_result = {}

segs = []			# 'fi index' in activate
injectPlaces = []		# 'fi inject instance' in activate  
classFile = [None] * fiTotal	# 3 types: 'masked' 'sdc' 'crash'
FRRN = []			# Final Relative Residual Norm if there exist
REG = []			
IP = []  			# 'Activate' in activate, short for instruction pointer ,used to match with assembly code

def errorFile(desdir):		
	fileDir =desdir+ "error_output"
	# get file names in this dir and return array
	fileName_list = os.listdir(fileDir)
	# change to string
	if len(fileName_list) == 0:
		return
	fileName = str(fileName_list)
	fileName = fileName.replace("errorfile-","").replace(" ","").replace("'","").replace("[","").replace("]","")
	global errorFileIndexes, duePerSeg, duePercent
	errorFileIndexes = list(int(char) for char in fileName.split(","))  # change string to int array
	# print errorFileIndexes

def amg_progFile(desdir):
	fileDir=desdir+"/prog_output"
	desline =  "FinalRelativeResidualNorm"
	fileName_list = os.listdir(fileDir)
	fileName = str(fileName_list)
	fileName = fileName.replace("outputfile-","").replace(" ","").replace("'","").replace("[","").replace("]","")
	progFileIndexes = list(int(char) for char in fileName.split(","))  # change string to int array
	
	global errorFileIndexes, classFile,FRRN
	
	# remove errorfile output
	for x in errorFileIndexes:
		progFileIndexes.remove(x)  
		classFile[x] = "crash"
		#FRRN[x] = None
	# print progFileIndexes
	for x in progFileIndexes:
		filePath=fileDir+"/outputfile-"+str(x)
		# print filePath
		with open(filePath,'r') as fp:
			lines = fp.readlines()
		normStr = []
		flag = False
		for line in lines:	
			line = str(line).replace(" ","")
			normStr = line.split('=')
			if normStr[0] == desline:
				flag = True
				break
		if flag is False:
			continue
		if normStr[-1] == 'nan\n' or normStr[-1] == '-nan\n':
			classFile[x] = "crash"
			#FRRN[x] = None
			continue
			#print 'hi'
		residnum = eval(normStr[-1])
		userDef = 1e-6
		if residnum > userDef:
			classFile[x] = "sdc"
		else:
			classFile[x] = "masked"
		#FRRN[x] = residnum	#new here
		

def miniFE_progFile(desdir):
	fileDir=desdir+"/prog_output"
	fileName_list = os.listdir(fileDir)
	fileName = str(fileName_list)
	fileName = fileName.replace("outputfile-","").replace(" ","").replace("'","").replace("[","").replace("]","")
	progFileIndexes = list(int(char) for char in fileName.split(","))  # change string to int array
	
	global errorFileIndexes, classFileN
        
	# remove errorfile output
	for x in errorFileIndexes:
		progFileIndexes.remove(x)
		classFile[x] = 'crash'
	# print progFileIndexes

	lineNumber = 16
	for x in progFileIndexes:
		filePath=fileDir+"/outputfile-"+str(x)
		#print filePath
		#print x
		with open(filePath,'r') as fp:
			lines = fp.readlines()
			resultline = lines[-1]
		resultline = str(resultline).replace(" ","")
		normStr = resultline.split(':')
		#print normStr[-1]
		if normStr[0] != "FinalResidNorm":
			continue
		if normStr[-1] == "nan\n" or  normStr[-1] == "-nan\n":
			continue
		residnum = eval(normStr[-1])
		userDef = 1e-6
		if residnum > userDef:
			classFile[x] = 'sdc'
		else:
			classFile[x] = 'masked'
		#FRRN[x] = residunum
	print('miniFE complete')

# HPL
def hpl_progFile(desdir):
	fileDir=desdir+"/prog_output"
	fileName_list = os.listdir(fileDir)
	fileName = str(fileName_list)
	fileName = fileName.replace("outputfile-","").replace(" ","").replace("'","").replace("[","").replace("]","")
	progFileIndexes = list(int(char) for char in fileName.split(","))  # change string to int array
	
	global errorFileIndexes, classFile
	# remove errorfile output
	for x in errorFileIndexes:
		progFileIndexes.remove(x)
		classFile[x] = "crash"
	#print progFileIndexes
	lineNumber = 53
	for x in progFileIndexes:
		filePath=fileDir+"/outputfile-"+str(x)
		resultline = linecache.getline(filePath,lineNumber).strip()
		if resultline.find("FAILED")>0:
			classFile[x]='sdc'
		if resultline.find("PASSED")>0:
			classFile[x]='masked'

#hpccg
def hpccg_progFile(desdir):
	fileDir=desdir+"/prog_output"
	fileName_list = os.listdir(fileDir)
	fileName = str(fileName_list)
	fileName = fileName.replace("outputfile-","").replace(" ","").replace("'","").replace("[","").replace("]","")
	progFileIndexes = list(int(char) for char in fileName.split(","))  # change string to int array

	global errorFileIndexes, classFile
	# remove errorfile output
	for x in errorFileIndexes:
		progFileIndexes.remove(x)
		classFile[x] = "crash"
	# print progFileIndexes
	lineNumber = 21
	for x in progFileIndexes:
		filePath=fileDir+"/outputfile-"+str(x)
		with open(filePath,'r') as fp:
			lines = fp.readlines()
		if len(lines) < lineNumber:
			continue
		resultline = lines[lineNumber]
		resultline = str(resultline).replace(" ","")
		normStr = resultline.split(':')
		if normStr[0] != "Finalresidual":
			continue
		residnum = eval(normStr[-1])
		userDef = 1e-6
		if residnum > userDef:
			classFile[x] = "sdc"
		else:
			classFile[x] = "masked"

def xsbench_progFile(desdir):
	print("xsbench_progFile start!")
	fileDir=desdir+"/prog_output"
	errDir = desdir + "/error_output"
	fileName_list = os.listdir(fileDir)
	fileName = str(fileName_list)
	fileName = fileName.replace("outputfile-","").replace(" ","").replace("'","").replace("[","").replace("]","")
	progFileIndexes = list(int(char) for char in fileName.split(","))  # change string to int array

	global errorFileIndexes, classFile

	# remove errorfile output
	for x in errorFileIndexes:
		# error file path
		errfile_name = "errorfile-" + str(x)
		file_path = os.path.join(errDir,errfile_name)  

		try:
			with open(file_path, 'r') as file:
				first_line = file.readline().replace("\n",'')
				return_code = first_line.split(" ")[-1]
				#if '1' in return_code and '3' not in return_code:
				if return_code == '1':
					#print("erroroutput-{}: return code {}\t\tsdc".format(x,return_code))
					classFile[x] = 'sdc'
				else:
					#print("erroroutput-{}: return code {}".format(x,return_code))
					classFile[x] = 'crash'
		except FileNotFoundError:
			print('file {} not exist'.format(file_path))
		except Exception as e:
			print('open error: {}'.format(e))
		
		progFileIndexes.remove(x)
		
	# print classFile
	# print progFileIndexes
	for x in progFileIndexes:
		classFile[x] = 'masked'
	# print classFile



def progFile(desdir):
	fileDir=desdir+"/prog_output"
	fileName_list = os.listdir(fileDir)
	fileName = str(fileName_list)
	fileName = fileName.replace("outputfile-","").replace(" ","").replace("'","").replace("[","").replace("]","")
	progFileIndexes = list(int(char) for char in fileName.split(","))  # change string to int array
			
	global errorFileIndexes, classFile
        
	# remove errorfile output
	for x in errorFileIndexes:
		progFileIndexes.remove(x)
		classFile[x] = 'crash'
	# print classFile
	# print progFileIndexes
	for x in progFileIndexes:
		classFile[x] = 'masked'
	# print classFile
	
def extract_strings(input_string):
    pattern = re.compile(r'name(\w+)in(\w+)')

    match = pattern.search(input_string)

    if match:
        group1 = match.group(1)  
        return group1
    else:
        return None


def getFIplace(dir):
	global classFile,REG,IP
	filepath=dir+"/activate"
	with open(filepath,'r') as fp:
		lines = fp.readlines()
        # save inject place
	count = 0
	flag = 0	
	#use flag to sentence a inner cycle.
	#flag change to 1 when starting an inner cycle by meeting "fiindex",and change to 0 once meet "Activated"
	for line in lines:
		line = line.replace(" ","")
		strs = line.split(':')
		if "latency" in line  :
			continue
		if strs[0] == "fiindex" and flag == 1 : # new inner cycle comes but can not find IP,attention ! this flag is an old flag of last loop
			segs.append(int(strs[-1]))
			IP.append('NoIP')
			#print('empty in:',segs[-2])
			flag = 1 # a inner cycle start
			continue
		if strs[0] == "fiindex" and flag == 0 : # last inner cycle have a proper IP(flag =0)
			segs.append(int(strs[-1]))
			flag = 1 # a inner cycle start
			continue
		if strs[0] == "fiinjectinstance" and flag == 1  :
			injectPlaces.append(int(strs[-1]))	
			continue
		if strs[0] == "Activated" and flag ==1:	# first "Activated" in an inner cycle
			tmpstr = strs[-1]
			ins_pointer=tmpstr[-7:-1]
			ins_pointer = str(ins_pointer)
			ins_pointer = str('0x'+ins_pointer)
			reg = extract_strings(str(tmpstr))
			#print reg
			if "Memoryinjection" in line :
				ins_pointer = 'NoIP'
				#print("Memoryinjection")
			
			IP.append(str(ins_pointer))
			REG.append(str(reg))
			count=count+1
			
#			print('percent: {:.0%}'.format(count/float(15000))))
			

			flag = 0        #inner cycle end with an IP
			continue
		if strs[0] == "Activated" and flag ==0:	# extra IP
			tmpstr = strs[-1]
			ins_pointer=tmpstr[-7:-1]
			ins_pointer = str('0x'+ins_pointer)
			reg = extract_strings(str(tmpstr))
			if "Memoryinjection" in line :
				ins_pointer = ''
				#print("Memor`yinjection")
			IP.append(str(ins_pointer))
			REG.append(str(reg))

			segs.append(segs[-1])
			injectPlaces.append(injectPlaces[-1])
			tmp = classFile[segs[-1]]
			classFile.insert(int(segs[-1]),tmp)
			continue
		if "NotActivated" in line  :
			IP.append("NoIP")
			#print("None",segs[-1])
			continue

def getTotalInstcount(desdir):
	path=desdir+"/pin.instcount.txt"
	file=open(path,"r")
	line=file.readline()
	countstr=line.split(":")[1]
	file.close()
	return int(countstr)


def saveResults(desdir):
	global segs,injectPlaces,classFile,REG
	df_per_fi_result["seg"] = segs
	df_per_fi_result["fi"] = injectPlaces
	df_per_fi_result["result_class"] = classFile
	#df_per_fi_result["residual_Num"] = FRRN
	df_per_fi_result["IP"] = IP
	df_per_fi_result["REG"] = REG
	
	# print classFile

	df1 = DataFrame.from_dict(df_per_fi_result,orient='index')
	df1 = df1.T 
	df1 = df1.dropna(axis=0, how='any')
	df1.to_csv(desdir+"/perFiResult.csv",index=0) 
	print(" complete")

def choose_progFile(app_folder):
	if sys.argv[1] == './' :
		app_name = sys.argv[2]
	else : 
		app_name = sys.argv[1]
	if "hpl" in app_name :
		hpl_progFile(app_folder)
	elif "amg" in app_name:
		amg_progFile(app_folder)
	elif "miniFE" in app_name:
		miniFE_progFile(app_folder)
	elif "hpccg" in app_name:
		hpccg_progFile(app_folder)
	elif "xsbench" in app_name:
		xsbench_progFile(app_folder)
	else :
		progFile(app_folder)
	print (app_name,'in',app_folder)

if __name__ == '__main__':
	if len(sys.argv) < 2 :
		print("please enter right arguments:app_path('./' for recommend),app_name")
		sys.exit()
	desdir = sys.argv[1]
	appname = sys.argv[2]
	errorFile(sys.argv[1])	
	choose_progFile(sys.argv[1])# before _proFile can be | amg_ | miniFE_ | hpccg_ | hpl_ |
	getFIplace(sys.argv[1])
	saveResults(sys.argv[1])
#	getTotalInstcount(sys.argv[1])	
