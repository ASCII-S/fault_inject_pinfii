#---------
# This file is used to find ins and ins_p from disassemblycode.txt with ip in perFiResult.csv
# To make graph through csv, check next .py
#---------
from __future__ import division
from __future__ import with_statement
from __future__ import absolute_import
import sys
import pandas as pd
from sklearn.preprocessing import StandardScaler
from io import open

filename = u"perFiResult.csv"
disassembly_filename = u'disassemblycode.txt'
nowapp = sys.argv[2]
sdcapp = [u'hpl']

# Formulate csv and cut NoIP
df = pd.read_csv(filename)
df = pd.DataFrame(df)
#df["IP"] = [row.replace('"','').replace("'","").replace('(','').replace(')','').replace('0x','').replace(',','')
#	for row in df["IP"]]
print u'len of raw data:',len(df)
list = [u'gggggg',u'NoIP']
df = df[df[u"IP"].unicode.contains(u"gggggg") == False]
df = df[df[u"IP"].unicode.contains(u"NoIP") == False]
#print u'len of after cutting \'noip\' data:',len(df)
data = df.sort_values(by=u"IP" , ascending=True)
data = data.reset_index(drop=True)

# below claim new column
INS_FULL = []
INS = [] 
FUC = []	#which FUC one IP belongs to

DE = []
S_Masked = []
S_Crash = []
S_Sdc = []
S_MS = []

def find_in_dac():
	global INS,INS_FULL,FUC,data
	# find more in dissassemblycodes.txt,such as INS_FULL,INS,and FUC
	filepath=u'./'+u'disassemblycode.txt'
        with open(filepath,u'r') as f:
		lines = f.readlines()
	countl =len(data)
	count = 0 
	for line in lines:
		if count >= countl:
			break
		if u'0000000000' in line[0:10]:
			fuc = line[17:-2]
		ip = data.loc[count,u"IP"]
		ip = ip.replace(u'0x',u'')
		if ip in line:
			tmp =line[32:62]
			tmp =u' '.join(tmp.split())
                        if len(tmp.replace(u' ',u''))==0 :         #special condition like 0000000000400ae0 <fopen@plt>
                                continue
			tmp = tmp.split(u' ')
                        ins_full = unicode(tmp).replace(u'\n',u'').replace(u'\r',u'').replace(u'"',u'').replace(u'[',u'').replace(u']',u'').replace(u"'",u"")
			ins = tmp[0]
			INS_FULL.append(ins_full)
                        INS.append(ins)
			FUC.append(fuc)
				
			print FUC[-1],u'\t',count,u'\t',ip,ins,ins_full
			
			count += 1
			if count < countl:
				next_ip = data.loc[count,u"IP"].replace(u'0x',u'')
				while next_ip == ip :	# same ip followed
					if count == countl:
                                                break
					INS_FULL.append(ins_full)
					INS.append(ins)
					FUC.append(fuc)
					count += 1
					if count == countl:
						break
					next_ip = data.loc[count,u"IP"].replace(u'0x',u'')
					#print FUC[-1],'\t',count,'\t',ip,ins,ins_full
	#print u"INS data:", len(INS),len(data)
	data[u'INS'] = INS 
        data[u'INS_FULL'] = INS_FULL
        data[u'FUC'] = FUC
						
def min_max_normalize(lst):
    lst = [float(x) for x in lst]
    min_val = min(lst)
    max_val = max(lst)
    normalized_lst = [round((x - min_val) / (max_val - min_val)*100.0,6) for x in lst]
    return normalized_lst

def smooth_data(input_data, window_size=3):
    smoothed_data = []

    for i in xrange(len(input_data)):
        start_index = max(0, i - window_size // 2)
        end_index = min(len(input_data), i + window_size // 2 + 1)

        average_value = 100.0 * sum(input_data[start_index:end_index]) / (end_index - start_index) * 1.0 

        smoothed_data.append(average_value)

    return smoothed_data



def math_column():
	# do some math analysis
	global data,DE,S_Masked,S_Crash,S_Sdc,S_MS,nowapp,sdcapp
	
	data[u'Masked'] = data[u'result_class'].map({u'masked':1,u'crash':0,u'sdc':0})
        data[u'Crash'] = data[u'result_class'].map({u'masked':0,u'crash':1,u'sdc':0})
	if nowapp in sdcapp:
		data[u'Sdc'] = data[u'result_class'].map({u'masked':0,u'crash':0,u'sdc':1})
		data[u'Masked+Sdc'] = data[u'result_class'].map({u'masked':1,u'crash':0,u'sdc':1})
	
	DE = [] * len(data) 
	data=data.sort_values(by=u"fi" , ascending=True)
	DE = min_max_normalize(data[u'fi'])
	
	S_Masked = smooth_data(data[u'Masked'],200)
	S_Crash = smooth_data(data[u'Crash'],200)
	if nowapp in sdcapp:
		S_Sdc = smooth_data(data[u'Sdc'],200)
		S_MS = smooth_data(data[u'Masked+Sdc'],200)
def save_list():
	# save all the list ro dataframe
	global data,nowapp,sdcapp
        data[u'Dynamic Execution'] = DE
        data[u'Masked%'] = S_Masked
        data[u'Crash%'] = S_Crash
	if nowapp in sdcapp:
	        data[u'Sdc%'] = S_Sdc
		data[u'Masked%+Sdc%'] = S_MS
		data=data.loc[:,[u'IP',u'REG',u'fi',u'seg',u'result_class',u'FUC',u'INS',u'INS_FULL',u'Masked',u'Crash',u'Sdc',u'Dynamic Execution',u'Masked%',u'Crash%',u'Sdc%',u'Masked%+Sdc%']]
	else :
		data=data.loc[:,[u'IP',u'REG',u'fi',u'seg',u'result_class',u'FUC',u'INS',u'INS_FULL',u'Masked',u'Crash',u'Dynamic Execution',u'Masked%',u'Crash%']]

        data.to_csv(u'Format_Result.csv', mode=u'w', index=False)
	

if __name__ == u'__main__':
	find_in_dac()
	
	math_column()
	#print sys.argv[2],len(data[u'fi']),len(S_MS),len(data)
	save_list()
	#print u"csv process complete!"

