UTF - a low-level communication library using uTofu
      		  			  2020/07/30

(0) Building utofu simulation environment on x86 and aarch64
   1) installation of simulator
     $ (cd new-sim-utofu; make; make install)

(1) Installation
   # UTF sources will be registered shortly
   # 1) installation of UTF
   #   $ (cd src/basic; make; make install)

(2) Experiment
   0) export UTF_ARCH environment variable
      For fugaku,
        $ export UTF_ARCH=fugaku
      For x86,
        $ export UTF_ARCH=x86
   1) A program code for measuring costs of the utofu library is available.
     $ (cd experiment/src; make; make install)
     job scripts are located in the experiment/run directory.
     e.g.
       $ cd experiment/run
       $ pjsub run-numazu-test_utofu.sh
       A result is stored in the ./resuts directory.

--
