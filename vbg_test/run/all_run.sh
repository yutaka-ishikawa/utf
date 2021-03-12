#!/bin/sh

JOB_SAMPLE="pjsub-sample.sh"
PJSUB_SCRIPT="run-fugaku-test-job-tmp.sh"
PJSTAT_MAX_NUM="2"
CSV_FILE_OUT="ON"

# Read TP.list
if [ $# = 0 ]; then
    TP_LIST="TPlist.txt"
else
    TP_LIST="$1"
fi

# Back up TP_list.txt
TPLIST_OUT="${TP_LIST%.txt}_out.txt"
cp ${TP_LIST} ${TPLIST_OUT}

# Create results-folder
RESULT_FOLDER=results-${TP_LIST%.txt}-`date '+%F'`
if [ ! -e ${RESULT_FOLDER} ]; then
    mkdir ${RESULT_FOLDER}
fi

# Create result file
if [ ${CSV_FILE_OUT} = "ON" ]; then
  RESULT_FILE="${RESULT_FOLDER}.csv"
  echo -n > ${RESULT_FILE}
fi

# Check result func
result_check(){

    NODE_JOBID=`pjstat -s --choose jid,jnam --filter "jnam=UTF-VBG" | grep "^ JOB ID" | sed "s/ JOB ID *: \([0-9]*\)/\1/g"`

   while true
    do
        PJSTAT_WC=`pjstat | grep UTF-VBG |wc -l`
        ERR_JOBID=`pjstat -s --choose rscg,st --filter "st=ERR" | grep -c -w "ERR"`
        ALL_STATUS=`expr ${PJSTAT_WC} - ${ERR_JOBID}`

        if [ "${ALL_STATUS}" = "0"  ]; then
            for i in ${NODE_JOBID[@]}
            do
                outfile=`find . -type f -name UTF-VBG.*.$i.out | sed 's!^.*/!!'`
                errfile=`find . -type f -name UTF-VBG.*.$i.err | sed 's!^.*/!!'`
                if [ -f "$RESULT_FOLDER/$outfile" ]; then
                    TP_NUM=`echo "${outfile}" | cut -d "." -f2`
                    out_status=`wc -c ${RESULT_FOLDER}/${outfile} | cut -d ' ' -f1 -`
                    err_status=`wc -c ${RESULT_FOLDER}/${errfile} | cut -d ' ' -f1 -`

                    if [ ${out_status} != "0" ]; then
                        out_result=`grep , ${RESULT_FOLDER}/${outfile}`
                        echo "${TP_NUM},$i,${out_result}"
                        if [ ${CSV_FILE_OUT} = "ON" ]; then
                            echo "${TP_NUM},$i,${out_result}" >> ${RESULT_FILE}
                        fi
                    else
                        if [ ${err_status} != "0" ]; then
                            echo "${TP_NUM},$i,  error,     -"
                            if [ ${CSV_FILE_OUT} = "ON" ]; then
                                echo "${TP_NUM},$i,  error,     -" >> ${RESULT_FILE}
                            fi
                        else
                            echo "${TP_NUM},$i,  timeout,     -"
                            if [ ${CSV_FILE_OUT} = "ON" ]; then
                                echo "${TP_NUM},$i,  timeout,     -" >> ${RESULT_FILE}
                            fi
                        fi
                    fi

                    # Update TPlist
                    out_status1=`grep -w correct ${RESULT_FOLDER}/${outfile} | wc -l`
                    out_status2=`grep passed ${RESULT_FOLDER}/${outfile} | wc -l`
                    if [ ${out_status1} = "1" ] && [ ${out_status2} = "1" ]; then
                        sed -i "s|${TP_NUM}|\#${TP_NUM}|g" ${TPLIST_OUT}
                    fi
                else
                    statsfile=`find . -type f -name UTF-VBG.$i.stats | sed 's!^.*/!!'`
                    if [ -f "${RESULT_FOLDER}/${statsfile}" ]; then
                        stats_status=`grep "ELAPSE LIMIT EXCEEDED" ${RESULT_FOLDER}/${statsfile} | wc -l`
                        if [ ${stats_status} = "1" ]; then
                            echo ",$i,  timeout,     -"
                            if [ ${CSV_FILE_OUT} = "ON" ]; then
                                echo ",$i,  timeout,     -" >> ${RESULT_FILE}
                            fi
                        else
                            echo ",$i,  error,     -"
                            if [ ${CSV_FILE_OUT} = "ON" ]; then
                                echo ",$i,  error,     -" >> ${RESULT_FILE}
                            fi
                        fi
                    fi
                fi
            done
            break
        else
            sleep 10
        fi
    done
}


cat pjsub.env | grep -v ^# | grep -v ^$ | while read line
do
    PJSUB_OP=${line}
    cat ${TP_LIST} | grep -v ^# | grep -v ^$ | while read line2
    do
        TP_NUM=`echo ${line2} | cut -d ' ' -f1`
        TP_ARG=`echo ${line2} | cut -d ' ' -f2-`
        cp ${JOB_SAMPLE} ${PJSUB_SCRIPT}
        sed -i "s|PJSUB_OP|-ofout ${RESULT_FOLDER}/\${PJM_JOBNAME}.${TP_NUM}.\${PJM_JOBID}.out -oferr ${RESULT_FOLDER}/\${PJM_JOBNAME}.${TP_NUM}.\${PJM_JOBID}.err|g" ${PJSUB_SCRIPT}
        sed -i "s|ARGUMENT|${TP_ARG}|g" ${PJSUB_SCRIPT}
        PJSUB_OPTIONS="-N UTF-VBG -S --spath ${RESULT_FOLDER}/%n.%j.stats -o ${RESULT_FOLDER}/%n.%j.out -e ${RESULT_FOLDER}/%n.%j.err"
        cmd="pjsub ${line} ${PJSUB_OPTIONS} ${PJSUB_SCRIPT}"
        #echo "$cmd"
        $cmd

        sleep 1
        # Check INSUFF_NODE
        INSUFF_NODE_JOBID=`pjstat -s --choose jid,st,ermsg --filter "ermsg=INSUFF *" | grep "^ JOB ID" | sed "s/ JOB ID *: \([0-9]*\)/\1/g"`
        if [ -n "${INSUFF_NODE_JOBID}" ]; then
            echo "pjdel "${INSUFF_NODE_JOBID}
            pjdel ${INSUFF_NODE_JOBID}
        fi

        # Check PJSUB
        RET=`pjstat | grep UTF-VBG |wc -l`

        # Check ERR JOB
        ERR_JOBID=`pjstat -s --choose rscg,st --filter "st=ERR" | grep -c -w "ERR"`

        ALL_STATUS=`expr ${RET} - ${ERR_JOBID}`
        if [ ${ALL_STATUS} -lt ${PJSTAT_MAX_NUM} ]; then
            continue
        fi
        result_check
    done
    result_check
done
