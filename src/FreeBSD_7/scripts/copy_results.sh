#!/bin/bash

PKT_SIZE=$1
PKT_COUNT=$2
DST_DIR=$3
TABLES=tables_SMP/
PLOTS=plots_SMP/


[ ! -d ${DST_DIR} ] && mkdir ${DST_DIR}
[ ! -d ${DST_DIR}/tables/ ] && mkdir ${DST_DIR}/${TABLES}
[ ! -d ${DST_DIR}/plots/ ] && mkdir ${DST_DIR}/${PLOTS}
TABLES_DIR=../../../${TABLES}

PLOTS_DIR=../../../${PLOTS}

# copy tables
cp ${TABLES_DIR}${PKT_SIZE}_${PKT_COUNT}_* ${DST_DIR}/${TABLES}
cp ${TABLES_DIR}${PKT_COUNT}_* ${DST_DIR}/${TABLES}

# copy plots
cp ${PLOTS_DIR}${PKT_SIZE}_${PKT_COUNT}_* ${DST_DIR}/${PLOTS}

exit 0
