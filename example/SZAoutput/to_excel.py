import pandas as pd 
import os
import sys

data = 'result'
path = './'+sys.argv[1]
files = os.listdir(path)

def save_to_excel():
	# gather all csv to one excel
	global data,path,files
	writer = pd.ExcelWriter(data+'.xls')
	print 'now gather all .csv to  one .xls:'
	print 
	for file in files:
		csvfile = path+'/'+file
		data = pd.read_csv(csvfile)
		df =pd.DataFrame(data)	
		

		df.to_excel(writer, sheet_name=file, index=False)
		writer.save()
		writer.close()
		
		print csvfile


if __name__ == '__main__':
	save_to_excel()
