SRC_DIR=.
SRC_MPICH=/home/z30485/mpich-tofu-org
DST_HOMEDIR=/data/01/pz0485/z30485
DST_RUNDIR=$(DST_HOMEDIR)/work/mpich-org
DST_BINDIR=$(DST_HOMEDIR)/work/bin-org

all:
copy:
	cp $(SRC_DIR)/*.sh $(DST_RUNDIR)
	cp $(SRC_DIR)/Makefile $(DST_RUNDIR)
	cp $(SRC_DIR)/../bin/* $(DST_BINDIR)
##bin-org-copy:
##	cp -rp $(SRC_DIR)/../bin $(DST_RUNDIR)/../bin-org
##	cd ../src; make org-install
mpich-copy:
	cp -r $(SRC_MPICH) $(DST_HOMEDIR)
results-save:
	cp -rp $(DST_RUNDIR)/results $(SRC_MPICH)/
clean:
	rm -f results/mpich-org/bcast/*
	rm -f results/mpich-org/imb/bcast/* || true
	rm -f pingpong-timing-*.txt
