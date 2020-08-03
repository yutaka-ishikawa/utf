		Howe to use Fujitsu profiler
								2020/08/03

https://docs.fugaku.r-ccs.riken.jp/en/user_guides/lang_2020_03/FujitsuCompiler/Profiler/index.html
https://docs.fugaku.r-ccs.riken.jp/ja/user_guides/lang_2020_03/FujitsuCompiler/Profiler/index.html

(1) A sample program is test_profile.c
   1) The "fj_tool/fapp.h" include file must be included.
   2) A section you want to measure is surrounded by
	fapp_start(<string>, 1, 0)  and fapp_stop(<string>, 1, 0) 
     A string represents a section name.
(2) Compilation.
    The compile option might be
    	 -Nclang -ffj-fjprof
    To compile the "test_profile.c" code, just type make.
    $ make
(3) A sample batch job script is run-numazu-testprofile.sh
    The "fapp" profile tool must be executed several times with different paramters.
    Just submit this job script.
    BE SURE that NO rep[1-15] directories exist under the profdata/ directory.
    $ pjsub run-numazu-testprofile.sh
    In this sample, 17 data are collected and stored under the profdata/ directory.
    Directories rep1 to re17 are created under the profdata directory.
    For MPI programs, please see the run-numazu-testmpiprofile.sh.
    No sample source code is available.
(4) A post processing script is conv.sh and conv1.sh
    conv.sh)
       In case of single or Fujitu's MPI applications, just use conv.sh.
       $ ./conv.sh
       pa[1-15].csv files are created under the profdata/ directory
    conv2.sh)
       In case of  RIKEN-MPICH applications, just use conv1.sh.
       $ ./conv1.sh
       pa[1-15].csv files are created under the profdata/ directory
(5) Analysis
    1) cp /opt/FJSVxtclanga/tcsds-1.2.25/misc/cpupa/cpu_pa_report.xlsm to your PC.
    2) cp all pa[1-15].csv files to the same folder of cpu_pa_report.xlsm your PC.
    3) Please read the following document for how to use this xlsm file with cvs files:
	https://docs.fugaku.r-ccs.riken.jp/ja/user_guides/lang_2020_03/FujitsuCompiler/Profiler/CPUPerformanceAnalysisReportCreateReport.html
      or
	https://docs.fugaku.r-ccs.riken.jp/ja/user_guides/lang_2020_03/FujitsuCompiler/Profiler/CPUPerformanceAnalysisReportCreateReport.html

--
