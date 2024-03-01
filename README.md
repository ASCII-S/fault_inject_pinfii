# BRIEF INFO
## Benchmark List :
	amg backprop bfs FFT HPCCG hpl knn lu miniFE
# HOW TO DO FI
## Sources
1. faultinject.py 
	This file is used to inject fault in different application
2. HPL.dat
	The input config of benchmark hpl 
3. disassemblycodes
	The assembly codes of different applications.
## sh file and py file
1. runFile.sh  
	Use this sh to do ```python faultinject.py $1 $runtime```  
	Followed with arguments application_folder and official_or_not(1 for official) if need to run officially  
	Such as:  
		``` sh runFile.sh ./hpl 0 ``` to do faultinject.py on hpl for test
		``` sh runFile.sh ./hpl 1 ``` to do an official fault injection on hpl
2. run_in_background.sh  
	Use this sh to nohup runFile.sh as backrun.  
	Followed with arguments same as runFIle.sh.  
3. archive_in_pinfii.sh  
	Collect all the output of applications in one folder called archive_data.  
	Such as:
		``` sh archive_in_pinfii.sh 1201 ``` to make folder archive_1201 and move all the folders of applications to archive_1201.  

4. ./SZAoutput/output_classification.py  
	Use this py to analysis in `one` application and generate an table of csv.  
	It can generate `perFiResult.csv` including  seg,fi,result_class,REG,IP  from each application.  
	Such as:
		``` python ./SZAoutput/output_classification.py ./ hpl ``` to analysis the output of hpl,(./ means do file in current folder,and hpl means the application name is hpl)  
5. ./SZAoutput/cpdisassemblycode.sh  
copy the disassemblycode to corresponding folders 
6. ./SZAoutput/csv_processor.py  
this py file find more information based on the cross of dissamblycodes and `perFiResult.csv`,after this process Format_Result.csv generated coming with more lines:  
IP	REG	fi	seg	result_class	FUC	INS	INS_FULL	Masked	Crash	Dynamic Execution	Masked%	Crash%
7. ./SZAoutput/all_in_one.sh  
   	combine output_classification.py and csv_processor.py together,can deal with all benchmarks and generate csv in folders respectively,  
   	usage:1 for the former and 2 for the latter,`python all_in_one.sh 1`  
10. ./SZAoutput/savecsv.sh  
	Save csv of each application to current and your own folder(can edit in savecsv.sh).usage:`sh savecsv.sh $data$`  
11. ./SZAoutput/count_progs.sh  
    	count the process of the running of ../benchmark
## run process 
make sure your folder arch:  
./ : amg backprop bfs FFT HPCCG hpl knn lu miniFE worktable  
worktable(SZAoutput) :all_in_one.sh         csv_processor.py          savecsv.sh	count_progs.sh        output_classification.py  to_excel.py	cpdisassemblycode.sh  

`run_in_background.sh`		--> |wait your application complete|  
`count_progs.sh`	--> |check process of running|  
`output_classification.py/all_in_one.sh 1` --> |now you have perFiResult.csv|  
`csv_processor.py/all_in_one.sh 2`	--> |Format_Result.csv|  
`savecsv.py` 			--> |now csv save in your own folder|  
`archive_in_pinfii.sh`		--> |archive all output in folder archive_##|  

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
