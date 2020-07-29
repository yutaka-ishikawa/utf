rm -f *.mypid *.stadd* *.vcq*
PROG="../src/test_utofu"

rm -f ./bfile.lck
export LD_LIBRARY_PATH=/home/ishikawa/utf/lib
export UTOFU_WORLD_ID=100
export UTOFU_TCP_SERVER_NAME=192.168.222.100
export UTOFU_TCP_SERVER_PORT=60000
export UTOFU_TCP_INTERFACE=ens33
export UTOFU_TCP_PORT=60100
export JTOFU_MAPINFO_NODE=1x2x1

$HOME/utf/bin/tsim_server \
    mpiexec.hydra -np 2 -f ./myhosts \
    -iface ${UTOFU_TCP_INTERFACE} \
    -launcher ssh \
    -genv LD_LIBRARY_PATH       ${LD_LIBRARY_PATH} \
    -genv UTOFU_WORLD_ID        ${UTOFU_WORLD_ID} \
    -genv UTOFU_TCP_SERVER_NAME ${UTOFU_TCP_SERVER_NAME} \
    -genv UTOFU_TCP_SERVER_PORT ${UTOFU_TCP_SERVER_PORT} \
    -genv UTOFU_TCP_INTERFACE   ${UTOFU_TCP_INTERFACE} \
    -genv UTOFU_TCP_PORT        ${UTOFU_TCP_PORT} \
    -genv JTOFU_MAPINFO_NODE    ${JTOFU_MAPINFO_NODE} \
    -genv LD_LIBRARY_PATH       ${LD_LIBRARY_PATH} \
    $PROG $1
