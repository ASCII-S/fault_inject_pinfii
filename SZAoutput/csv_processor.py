#---------
# This file is used to find ins and ins_p from disassemblycode.txt with ip in perFiResult.csv
# To make graph through csv, check next .py
#---------
import sys
import pandas as pd
from sklearn.preprocessing import StandardScaler

filename = "perFiResult.csv"
disassembly_filename = 'disassemblycode.txt'
nowapp = sys.argv[2]
tofilename = nowapp+'.csv'
sdcapp = ['hpl']

# Formulate csv and cut NoIP
df = pd.read_csv(filename)
df = pd.DataFrame(df)
#df["IP"] = [row.replace('"','').replace("'","").replace('(','').replace(')','').replace('0x','').replace(',','')
#	for row in df["IP"]]
print 'len of raw data:',len(df)
list = ['gggggg','NoIP']
df = df[df["IP"].str.contains("gggggg") == False]
df = df[df["IP"].str.contains("NoIP") == False]
print 'len of after cutting \'noip\' data:',len(df)
data = df.sort_values(by="IP" , ascending=True)
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
	filepath='./'+'disassemblycode.txt'
        with open(filepath,'r') as f:
                lines = f.readlines()
	countl =len(data)
	count = 0 
	for line in lines:
		if count >= countl:
			break
		if '0000000000' in line[0:10]:
			fuc = line[17:-2]
		ip = data.loc[count,"IP"]
		ip = ip.replace('0x','')
		if ip in line:
			tmp =line[32:62]
			tmp =' '.join(tmp.split())
                        if len(tmp.replace(' ',''))==0 :         #special condition like 0000000000400ae0 <fopen@plt>
                                continue
			tmp = tmp.split(' ')
                        ins_full = str(tmp).replace('\n','').replace('\r','').replace('"','').replace('[','').replace(']','').replace("'","")
			ins = tmp[0]
			INS_FULL.append(ins_full)
                        INS.append(ins)
			FUC.append(fuc)
				
			print FUC[-1],'\t',count,'\t',ip,ins,ins_full
			
			count += 1
			if count < countl:
				next_ip = data.loc[count,"IP"].replace('0x','')
				while next_ip == ip :	# same ip followed
					if count == countl:
                                                break
					INS_FULL.append(ins_full)
					INS.append(ins)
					FUC.append(fuc)
					count += 1
					if count == countl:
						break
					next_ip = data.loc[count,"IP"].replace('0x','')
					#print FUC[-1],'\t',count,'\t',ip,ins,ins_full
	print "INS data:", len(INS),len(data)
	data['INS'] = INS 
        data['INS_FULL'] = INS_FULL
        data['FUC'] = FUC
						
def min_max_normalize(lst):
    lst = [float(x) for x in lst]
    min_val = min(lst)
    max_val = max(lst)
    normalized_lst = [round((x - min_val) / (max_val - min_val)*100.0,6) for x in lst]
    return normalized_lst

def smooth_data(input_data, window_size=3):
    smoothed_data = []

    for i in range(len(input_data)):
        start_index = max(0, i - window_size // 2)
        end_index = min(len(input_data), i + window_size // 2 + 1)

        average_value = 100.0 * sum(input_data[start_index:end_index]) / (end_index - start_index) * 1.0 

        smoothed_data.append(average_value)

    return smoothed_data



def math_column():
	# do some math analysis
	global data,DE,S_Masked,S_Crash,S_Sdc,S_MS,nowapp,sdcapp,tofilename
	
	data['Masked'] = data['result_class'].map({'masked':1,'crash':0,'sdc':0})
        data['Crash'] = data['result_class'].map({'masked':0,'crash':1,'sdc':0})
	if nowapp in sdcapp:
		data['Sdc'] = data['result_class'].map({'masked':0,'crash':0,'sdc':1})
		data['Masked+Sdc'] = data['result_class'].map({'masked':1,'crash':0,'sdc':1})
	
	DE = [] * len(data) 
	data=data.sort_values(by="fi" , ascending=True)
	DE = min_max_normalize(data['fi'])
	
	S_Masked = smooth_data(data['Masked'],200)
	S_Crash = smooth_data(data['Crash'],200)
	if nowapp in sdcapp:
		S_Sdc = smooth_data(data['Sdc'],200)
		S_MS = smooth_data(data['Masked+Sdc'],200)
def save_list():
	# save all the list ro dataframe
	global data,nowapp,sdcapp
        data['Dynamic Execution'] = DE
        data['Masked%'] = S_Masked
        data['Crash%'] = S_Crash
	if nowapp in sdcapp:
	        data['Sdc%'] = S_Sdc
		data['Masked%+Sdc%'] = S_MS
		data=data.loc[:,['IP','REG','fi','seg','result_class','FUC','INS','INS_FULL','Masked','Crash','Sdc','Dynamic Execution','Masked%','Crash%','Sdc%','Masked%+Sdc%']]
	else :
		data=data.loc[:,['IP','REG','fi','seg','result_class','FUC','INS','INS_FULL','Masked','Crash','Dynamic Execution','Masked%','Crash%']]

        data.to_csv(tofilename, mode='w', index=False)
	

if __name__ == '__main__':
	find_in_dac()
	
	math_column()
	print sys.argv[2],len(data['fi']),len(S_MS),len(data)
	save_list()
	print "csv process complete!"

