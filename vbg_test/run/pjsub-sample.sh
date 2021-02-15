#!/bin/bash

. ./ct.env

mpich_exec PJSUB_OP ${EXEC_OPTION} ${LD} ARGUMENT
