# BRIEF INFO
## Benchmark List :
	amg backprop bfs FFT HPCCG hpl knn lu miniFE
## Folder Arch:
./	
disassemblycodes
pin.log      
run_in_background.sh
archive_*        
faultinject.py    
pintool.log  
SZAoutput
	all_in_one.sh   
	cpdisassemblycode.sh  
	output_classification.py
	savecsv.sh
	count_progs.sh  
	csv_processor.py      
	to_excel.py
archive_bigscale      
HPL.dat           
README       
TSYoutput
archive_in_pinfii.sh  
instcategory.py   
runFile.sh

# HOW TO DO FI
## Sources
1. faultinject.py 
	This file is used to inject fault in different application
2. HPL.dat
	The input config of benchmark hpl 
3. disassemblycodes
	The assembly codes of different application.
## shell file 
1. runFile.sh
	Use this sh to do ```python faultinject.py $1 $runtime``` 
	Followed with arguments application_folder and official_or_not(1 for official) if need to run officially
	Such as: 
		``` sh runFile.sh ./hpl ``` to faultinject.py hpl for test
		``` sh runFile.sh ./hpl 1 ``` to do an official fault injection for hpl
2. run_in_background.sh
	Use this sh to nohup runFile.sh as back run.
	Followed with arguments same as runFIle.sh.
3. ./SZAoutput/output_classification.py
	Use this py to analysis in each application and generate an table of csv.
	It can extract injectPlaces, classFile, Inst_Pointer,and catagories of IP from each application.
	Such as:
		``` python ./SZAoutput/output_classification.py ./hpl ``` to analysis the output of hpl
4. savecsv.sh
	Save csv of each application to your own folder.
5. archive_in_pinfii.sh
	Collect all the output of applications in one folder called archive_data.
	Such as:
		``` sh archive_in_pinfii.sh 1201 ``` to make folder archive_1201 and move all the folders of applications to archive_1201.
## run process 
```run_in_background.sh``` 	--> |wait your application complete| 
```output_classification.py``` 	--> |now you have perFiResult.csv| 
```savecsv.py``` 		--> |now you have Format_Result.csv|
```archive_in_pinfii.sh```	--> |archive all output in folder archive_##|

-----

# Input scale as followed(saved in faultinject.oy):
benchmark name 	| archive_1028/reference input	| now				|state
amg		| -n,5,5,5			| -n,5,5,5			|done
backprop	| 655360			| 655360			|
bfs		|				| 				|done
FFT		|				|				|doing
HPCCG		| nx=50 ny=50 nz=50 iter=150	| nx=50 ny=50 nz=50 iter=150	|doing
HPL		| N=1000			| N=1000			|done
knn		| '5', '30', '90'		| '5', '30', '90'		|
lu		| '-s512'			|				|
miniFE		| '20','30','10'		| '20','30','10'		|done
