#!/bin/sh
#############################################################################
# bash script buildpkb.sh --- builds textana and siggen pkb files
#                             from textana and siggen lingware source files
#
# Copyright (C) 2009 SVOX AG. All rights reserved.
#############################################################################

#tools
TOOLS_DIR=.

#textana sources
TA_SRC_DIR=../textana

#siggen sources
SG_SRC_DIR=../siggen

#resulting pkb
DEST_PKB_DIR=../pkb

#temporary files
TMP_DIR=.

####  check parameter count

if [ -n "$4" -o -z "$2" ]; then
    echo "usage: $0  <langid>  <countryid>  <voice>";
    echo "  e.g: $0 en GB kh0";
    exit;
fi

#### check if language supported

if [ $1 = "en" ]; then
    if [ $2 = "GB" ]; then
        echo
    elif [ $2 = "US" ]; then
        echo
    else
        echo;
        echo [ERROR] invalid country id for language \"$1\"; exit;
    fi
elif [ $1 = "de" ]; then
    if [ $2 = "DE" ]; then
        echo
    else
        echo;
        echo [ERROR] invalid country id for language \"$1\"; exit;
    fi
elif [ $1 = "es" ]; then
    if [ $2 = "ES" ]; then
        echo
    else
        echo;
        echo [ERROR] invalid country id for language \"$1\"; exit;
    fi
elif [ $1 = "it" ]; then
    if [ $2 = "IT" ]; then
        echo
    else
        echo;
        echo [ERROR] invalid country id for language \"$1\"; exit;
    fi
elif [ $1 = "fr" ]; then
    if [ $2 = "FR" ]; then
        echo
    else
        echo;
        echo [ERROR] invalid country id for language \"$1\"; exit;
    fi
else
    echo;
    echo [ERROR] invalid language id; exit;
fi

LANG=$1-$2
VOICE=$3

###########################################
##
## SET LANGUAGE-DEPENDENT PARAMS
##
###########################################

if [ $LANG = "en-GB" ]; then
    WPHO_RANGE="1"
    SPHO_RANGE="1 2 3 4"
    if [ "$VOICE" = "kh0" ]; then
        SPHO_VOICE_RANGE="5"
    else
        echo [ERROR] \<voice\> is needed;
        exit;
    fi
elif [ $LANG = "en-US" ]; then
    WPHO_RANGE="1 2"
    SPHO_RANGE="1"
    if [ "$VOICE" = "mh5" ]; then
        SPHO_VOICE_RANGE="2 3 4 5"
    elif [ "$VOICE" = "kr0" ]; then
        SPHO_VOICE_RANGE="2 3 4"
    elif [ "$VOICE" = "lh0" ]; then
        SPHO_VOICE_RANGE="2 3 4 5"
    else
        echo [ERROR] \<voice\> is needed;
        exit;
    fi
elif [ $LANG = "de-DE" ]; then
    WPHO_RANGE="1 2"
    SPHO_RANGE="1"
    if [ "$VOICE" = "gl0" ]; then
        SPHO_VOICE_RANGE="2"
    else
        echo [ERROR] \<voice\> is needed;
        exit;
    fi
elif [ $LANG = "es-ES" ]; then
    WPHO_RANGE="1"
    SPHO_RANGE="1 2 3 4"
    if [ "$VOICE" = "zl0" ]; then
        SPHO_VOICE_RANGE="5"
    else
        echo [ERROR] \<voice\> is needed;
        exit;
    fi
elif [ $LANG = "it-IT" ]; then
    WPHO_RANGE="1"
    SPHO_RANGE="1"
    if [ "$VOICE" = "cm0" ]; then
        SPHO_VOICE_RANGE="2"
    else
        echo [ERROR] \<voice\> is needed;
        exit;
    fi
elif [ $LANG = "fr-FR" ]; then
    WPHO_RANGE="1"
    SPHO_RANGE="1 2 3 4 5 6 7 8"
    if [ "$VOICE" = "nk0" ]; then
        SPHO_VOICE_RANGE="9"
    else
        echo [ERROR] \<voice\> is needed;
        exit;
    fi
fi
TA_SRC_DIR_LANG=${TA_SRC_DIR}/${LANG}/
SG_SRC_DIR_LANG=${SG_SRC_DIR}/${LANG}/

DEST_PKB_DIR_LANG=${DEST_PKB_DIR}/${LANG}/

## language-independent values
LFZ_RANGE="1 2 3 4 5"    
MGC_RANGE="1 2 3 4 5"    

###########################################
##
## DEFINE FILE NAMES
##
###########################################

#####  TABLES / LEXICON ###################

GRAPHS_FN=${LANG}_graphs.utf
PHONES_FN=${LANG}_phones.utf
POS_FN=${LANG}_pos.utf

LEXPOS_FN=${LANG}_lexpos.utf

GRAPHS_PKB_FN=${LANG}_ktab_graphs.pkb
PHONES_PKB_FN=${LANG}_ktab_phones.pkb
POS_PKB_FN=${LANG}_ktab_pos.pkb

LEXPOS_PKB_FN=${LANG}_klex.pkb

DBG_PKB_FN=${LANG}_kdbg.pkb

#####  AUTOMATA  ########################

# phonetic input alphabet parser
PARSE_XSAMPA=parse-xsampa

PARSE_XSAMPA_SYM_FN=${PARSE_XSAMPA}_symtab.utf
PARSE_XSAMPA_AUT_FN=${PARSE_XSAMPA}_aut.utf

PARSE_XSAMPA_AUT_PKB_FN=kfst_${PARSE_XSAMPA}.pkb

# phonetic input alphabet to internal phonemes mapper
MAP_XSAMPA=map-xsampa

MAP_XSAMPA_SYM_FN=${LANG}_${MAP_XSAMPA}_symtab.utf
MAP_XSAMPA_AUT_FN=${LANG}_${MAP_XSAMPA}_aut.utf

MAP_XSAMPA_AUT_PKB_FN=${LANG}_kfst_${MAP_XSAMPA}.pkb

for i in $WPHO_RANGE; do
    eval AUT_WORD${i}_FN=${LANG}_aut_wpho${i}.utf
    eval AUT_PKB_WORD${i}_FN=${LANG}_kfst_wpho${i}.pkb
done

for i in ${SPHO_RANGE}; do
    eval AUT_SENT${i}_FN=${LANG}_aut_spho${i}.utf
    eval AUT_PKB_SENT${i}_FN=${LANG}_kfst_spho${i}.pkb
done

# voice dependent FSTs
for i in ${SPHO_VOICE_RANGE}; do
    eval AUT_SENT${i}_FN=${LANG}_${VOICE}_aut_spho${i}.utf
    eval AUT_PKB_SENT${i}_FN=${LANG}_${VOICE}_kfst_spho${i}.pkb
done

PHONES_SYM_FN=${LANG}_phones_symtab.utf

#####  PREPROC  ###########################

TPP_NET_FN=${LANG}_tpp_net.utf
TPP_NET_PKB_FN=${LANG}_kpr.pkb

#####  DECISION TREES  ####################

#textana DT
DT_POSP_FN=${LANG}_kdt_posp.utf
DT_POSD_FN=${LANG}_kdt_posd.utf
DT_G2P_FN=${LANG}_kdt_g2p.utf
DT_PHR_FN=${LANG}_kdt_phr.utf
DT_ACC_FN=${LANG}_kdt_acc.utf

#textana configuration ini files
CFG_POSP_FN=${LANG}_kdt_posp.dtfmt
CFG_POSD_FN=${LANG}_kdt_posd.dtfmt
CFG_G2P_FN=${LANG}_kdt_g2p.dtfmt
CFG_PHR_FN=${LANG}_kdt_phr.dtfmt
CFG_ACC_FN=${LANG}_kdt_acc.dtfmt

DT_PKB_POSP_FN=${LANG}_kdt_posp.pkb
DT_PKB_POSD_FN=${LANG}_kdt_posd.pkb
DT_PKB_G2P_FN=${LANG}_kdt_g2p.pkb
DT_PKB_PHR_FN=${LANG}_kdt_phr.pkb
DT_PKB_ACC_FN=${LANG}_kdt_acc.pkb

#siggen DT
DT_DUR_FN=${LANG}_${VOICE}_kdt_dur.utf
DT_PKB_DUR_FN=${LANG}_${VOICE}_kdt_dur.pkb

for i in $LFZ_RANGE; do
    eval DT_LFZ${i}_FN=${LANG}_${VOICE}_kdt_lfz${i}.utf
    eval DT_PKB_LFZ${i}_FN=${LANG}_${VOICE}_kdt_lfz${i}.pkb
done    
for i in $MGC_RANGE; do
    eval DT_MGC${i}_FN=${LANG}_${VOICE}_kdt_mgc${i}.utf
    eval DT_PKB_MGC${i}_FN=${LANG}_${VOICE}_kdt_mgc${i}.pkb
done

#siggen configuration ini file
CFG_SG_FN=kdt_pam.dtfmt

#####  PDFS  ##############################

PDF_DUR_FN=${LANG}_${VOICE}_kpdf_dur.utf
PDF_LFZ_FN=${LANG}_${VOICE}_kpdf_lfz.utf
PDF_MGC_FN=${LANG}_${VOICE}_kpdf_mgc.utf
PDF_PHS_FN=${LANG}_${VOICE}_kpdf_phs.utf

PDF_PKB_DUR_FN=${LANG}_${VOICE}_kpdf_dur.pkb
PDF_PKB_LFZ_FN=${LANG}_${VOICE}_kpdf_lfz.pkb
PDF_PKB_MGC_FN=${LANG}_${VOICE}_kpdf_mgc.pkb
PDF_PKB_PHS_FN=${LANG}_${VOICE}_kpdf_phs.pkb

###########################################
##
## DEFINE FULL FILE PATHS
##
###########################################

#####  TABLES / LEXICA   ##################

SRC_GRAPHS=`cygpath -w $TA_SRC_DIR_LANG/$GRAPHS_FN`
DEST_PKB_GRAPHS=`cygpath -w $DEST_PKB_DIR_LANG/$GRAPHS_PKB_FN`

SRC_PHONES=`cygpath -w $TA_SRC_DIR_LANG/$PHONES_FN`
DEST_PKB_PHONES=`cygpath -w $DEST_PKB_DIR_LANG/$PHONES_PKB_FN`

SRC_POS=`cygpath -w $TA_SRC_DIR_LANG/$POS_FN`
DEST_PKB_POS=`cygpath -w $DEST_PKB_DIR_LANG/$POS_PKB_FN`

SRC_LEX=`cygpath -w $TA_SRC_DIR_LANG/$LEXPOS_FN`
DEST_PKB_LEX=`cygpath -w $DEST_PKB_DIR_LANG/$LEXPOS_PKB_FN`

DEST_PKB_DBG=`cygpath -w $DEST_PKB_DIR_LANG/$DBG_PKB_FN`

#####  AUTOMATA  ########################

SRC_PARSE_XSAMPA_AUT=`cygpath -w $TA_SRC_DIR/$PARSE_XSAMPA_AUT_FN`
SRC_PARSE_XSAMPA_SYM=`cygpath -w $TA_SRC_DIR/$PARSE_XSAMPA_SYM_FN`

DEST_PARSE_XSAMPA_AUT_PKB=`cygpath -w $DEST_PKB_DIR/$PARSE_XSAMPA_AUT_PKB_FN`

SRC_MAP_XSAMPA_AUT=`cygpath -w $TA_SRC_DIR_LANG/$MAP_XSAMPA_AUT_FN`
SRC_MAP_XSAMPA_SYM=`cygpath -w $TA_SRC_DIR_LANG/$MAP_XSAMPA_SYM_FN`

DEST_MAP_XSAMPA_AUT_PKB=`cygpath -w $DEST_PKB_DIR_LANG/$MAP_XSAMPA_AUT_PKB_FN`


for i in $WPHO_RANGE; do
    eval SRC_AUT_WORD${i}='"`'cygpath -w $TA_SRC_DIR_LANG/\$AUT_WORD${i}_FN'`"'
    eval DEST_AUT_PKB_WORD${i}='"`'cygpath -w $DEST_PKB_DIR_LANG/\$AUT_PKB_WORD${i}_FN'`"'
done

for i in $SPHO_RANGE; do
    eval SRC_AUT_SENT${i}='"`'cygpath -w $TA_SRC_DIR_LANG/\$AUT_SENT${i}_FN'`"'
    eval DEST_AUT_PKB_SENT${i}='"`'cygpath -w $DEST_PKB_DIR_LANG/\$AUT_PKB_SENT${i}_FN'`"'
done

#voice dependent FSTs
for i in $SPHO_VOICE_RANGE; do
    eval SRC_AUT_SENT${i}='"`'cygpath -w $TA_SRC_DIR_LANG/\$AUT_SENT${i}_FN'`"'
    eval DEST_AUT_PKB_SENT${i}='"`'cygpath -w $DEST_PKB_DIR_LANG/\$AUT_PKB_SENT${i}_FN'`"'
done

TMP_PHONES_SYM=`cygpath -w $TMP_DIR/$PHONES_SYM_FN`

#####  PREPROC  ###########################

SRC_TPP_NET=`cygpath -w $TA_SRC_DIR_LANG/$TPP_NET_FN`
DEST_TPP_NET_PKB=`cygpath -w $DEST_PKB_DIR_LANG/$TPP_NET_PKB_FN`

#####  DECISION TREES  ####################

#textana DT
SRC_DT_POSP=`cygpath -w $TA_SRC_DIR_LANG/$DT_POSP_FN`
SRC_DT_POSD=`cygpath -w $TA_SRC_DIR_LANG/$DT_POSD_FN`
SRC_DT_G2P=`cygpath -w $TA_SRC_DIR_LANG/$DT_G2P_FN`
SRC_DT_PHR=`cygpath -w $TA_SRC_DIR_LANG/$DT_PHR_FN`
SRC_DT_ACC=`cygpath -w $TA_SRC_DIR_LANG/$DT_ACC_FN`

#textana configuration ini files
CFG_DT_POSP=`cygpath -w $TA_SRC_DIR_LANG/$CFG_POSP_FN`
CFG_DT_POSD=`cygpath -w $TA_SRC_DIR_LANG/$CFG_POSD_FN`
CFG_DT_G2P=`cygpath -w $TA_SRC_DIR_LANG/$CFG_G2P_FN`
CFG_DT_PHR=`cygpath -w $TA_SRC_DIR_LANG/$CFG_PHR_FN`
CFG_DT_ACC=`cygpath -w $TA_SRC_DIR_LANG/$CFG_ACC_FN`

#siggen
SRC_DT_DUR=`cygpath -w $SG_SRC_DIR_LANG/$DT_DUR_FN`
    
for i in $LFZ_RANGE; do
    eval SRC_DT_LFZ${i}='"`'cygpath -w $SG_SRC_DIR_LANG/\$DT_LFZ${i}_FN'`"'
done
    
for i in $MGC_RANGE; do
    eval SRC_DT_MGC${i}='"`'cygpath -w $SG_SRC_DIR_LANG/\$DT_MGC${i}_FN'`"'
done

#siggen configuration ini file
CFG_DT_SG=`cygpath -w $SG_SRC_DIR/$CFG_SG_FN`

#textana
DEST_DT_PKB_POSP=`cygpath -w $DEST_PKB_DIR_LANG/$DT_PKB_POSP_FN`
DEST_DT_PKB_POSD=`cygpath -w $DEST_PKB_DIR_LANG/$DT_PKB_POSD_FN`
DEST_DT_PKB_G2P=`cygpath -w $DEST_PKB_DIR_LANG/$DT_PKB_G2P_FN`
DEST_DT_PKB_PHR=`cygpath -w $DEST_PKB_DIR_LANG/$DT_PKB_PHR_FN`
DEST_DT_PKB_ACC=`cygpath -w $DEST_PKB_DIR_LANG/$DT_PKB_ACC_FN`


#siggen
DEST_DT_PKB_DUR=`cygpath -w $DEST_PKB_DIR_LANG/$DT_PKB_DUR_FN`
    
for i in $LFZ_RANGE; do
    eval DEST_DT_PKB_LFZ${i}='"`'cygpath -w $DEST_PKB_DIR_LANG/\$DT_PKB_LFZ${i}_FN'`"'
done
    
for i in $MGC_RANGE; do
    eval DEST_DT_PKB_MGC${i}='"`'cygpath -w $DEST_PKB_DIR_LANG/\$DT_PKB_MGC${i}_FN'`"'
done


#####  PDFS  ##############################

SRC_PDF_DUR=`cygpath -w $SG_SRC_DIR_LANG/$PDF_DUR_FN`
SRC_PDF_LFZ=`cygpath -w $SG_SRC_DIR_LANG/$PDF_LFZ_FN`
SRC_PDF_MGC=`cygpath -w $SG_SRC_DIR_LANG/$PDF_MGC_FN`
SRC_PDF_PHS=`cygpath -w $SG_SRC_DIR_LANG/$PDF_PHS_FN`

DEST_PDF_PKB_DUR=`cygpath -w $DEST_PKB_DIR_LANG/$PDF_PKB_DUR_FN`
DEST_PDF_PKB_LFZ=`cygpath -w $DEST_PKB_DIR_LANG/$PDF_PKB_LFZ_FN`
DEST_PDF_PKB_MGC=`cygpath -w $DEST_PKB_DIR_LANG/$PDF_PKB_MGC_FN`
DEST_PDF_PKB_PHS=`cygpath -w $DEST_PKB_DIR_LANG/$PDF_PKB_PHS_FN`

###########################################
##
## Compilation into pkb
##
###########################################

SYMSHIFT=${TOOLS_DIR}/symshift.pl
GRAPHS_TO_PKB=${TOOLS_DIR}/picoloadgraphs.exe
PHONES_TO_PKB=${TOOLS_DIR}/picoloadphones.lua
DBG_TO_PKB=${TOOLS_DIR}/picoloaddbg.lua
POS_TO_PKB=${TOOLS_DIR}/picoloadpos.exe
LEX_TO_PKB=${TOOLS_DIR}/picoloadlex
AUT_TO_PKB=${TOOLS_DIR}/picoloadfst.exe
TPP_TO_PKB=${TOOLS_DIR}/picoloadpreproc.exe
DT_TO_PKB=${TOOLS_DIR}/dt2pkb.exe
PDF_TO_PKB=${TOOLS_DIR}/pdf2pkb.exe

##### build intermediate files ############################

echo ${SYMSHIFT} -phones $SRC_PHONES -POS $SRC_POS > $TMP_PHONES_SYM
${SYMSHIFT} -phones $SRC_PHONES -POS $SRC_POS > $TMP_PHONES_SYM

#####  TABLES  ############################

echo ${GRAPHS_TO_PKB}  $SRC_GRAPHS               ${DEST_PKB_GRAPHS}
${GRAPHS_TO_PKB}  $SRC_GRAPHS               ${DEST_PKB_GRAPHS}

echo ${PHONES_TO_PKB}  $SRC_PHONES               ${DEST_PKB_PHONES}
${PHONES_TO_PKB}  $SRC_PHONES               ${DEST_PKB_PHONES}

echo ${POS_TO_PKB}     $SRC_POS                  ${DEST_PKB_POS}
${POS_TO_PKB}     $SRC_POS                  ${DEST_PKB_POS}

echo ${DBG_TO_PKB}    $SRC_PHONES               ${DEST_PKB_DBG}
${DBG_TO_PKB}    $SRC_PHONES               ${DEST_PKB_DBG}

#####  LEXICON ############################

echo ${LEX_TO_PKB}         $SRC_PHONES $SRC_POS $SRC_LEX ${DEST_PKB_LEX}
${LEX_TO_PKB}         $SRC_PHONES $SRC_POS $SRC_LEX ${DEST_PKB_LEX}

#####  AUTOMATA  ########################

echo ${AUT_TO_PKB} ${SRC_PARSE_XSAMPA_AUT} ${SRC_PARSE_XSAMPA_SYM} $DEST_PARSE_XSAMPA_AUT_PKB
${AUT_TO_PKB} ${SRC_PARSE_XSAMPA_AUT} ${SRC_PARSE_XSAMPA_SYM} $DEST_PARSE_XSAMPA_AUT_PKB

echo ${AUT_TO_PKB} ${SRC_MAP_XSAMPA_AUT} ${SRC_MAP_XSAMPA_SYM} $DEST_MAP_XSAMPA_AUT_PKB
${AUT_TO_PKB} ${SRC_MAP_XSAMPA_AUT} ${SRC_MAP_XSAMPA_SYM} $DEST_MAP_XSAMPA_AUT_PKB

for i in $WPHO_RANGE; do
    eval echo ${AUT_TO_PKB} \${SRC_AUT_WORD${i}} ${TMP_PHONES_SYM} \${DEST_AUT_PKB_WORD${i}}
    eval ${AUT_TO_PKB} \${SRC_AUT_WORD${i}} ${TMP_PHONES_SYM} \${DEST_AUT_PKB_WORD${i}}
done

for i in $SPHO_RANGE; do
    eval echo ${AUT_TO_PKB} \${SRC_AUT_SENT${i}} ${TMP_PHONES_SYM} \${DEST_AUT_PKB_SENT${i}}
    eval ${AUT_TO_PKB} \${SRC_AUT_SENT${i}} ${TMP_PHONES_SYM} \${DEST_AUT_PKB_SENT${i}}
done

# voice dependent FSTs
for i in $SPHO_VOICE_RANGE; do
    eval echo ${AUT_TO_PKB} \${SRC_AUT_SENT${i}} ${TMP_PHONES_SYM} \${DEST_AUT_PKB_SENT${i}}
    eval ${AUT_TO_PKB} \${SRC_AUT_SENT${i}} ${TMP_PHONES_SYM} \${DEST_AUT_PKB_SENT${i}}
done

#####  PREPROC  ###########################

echo ${TPP_TO_PKB} $SRC_GRAPHS $SRC_TPP_NET $DEST_TPP_NET_PKB
${TPP_TO_PKB} $SRC_GRAPHS $SRC_TPP_NET $DEST_TPP_NET_PKB

#####  DECISION TREES  ####################

#textana
#config file is added as the argument of the dt2pkb tool
echo ${DT_TO_PKB}  $CFG_DT_POSP $SRC_DT_POSP $DEST_DT_PKB_POSP
${DT_TO_PKB}  $CFG_DT_POSP $SRC_DT_POSP $DEST_DT_PKB_POSP

echo ${DT_TO_PKB}  $CFG_DT_POSD $SRC_DT_POSD $DEST_DT_PKB_POSD
${DT_TO_PKB}  $CFG_DT_POSD $SRC_DT_POSD $DEST_DT_PKB_POSD

echo ${DT_TO_PKB}  $CFG_DT_G2P $SRC_DT_G2P $DEST_DT_PKB_G2P
${DT_TO_PKB}  $CFG_DT_G2P $SRC_DT_G2P $DEST_DT_PKB_G2P

echo ${DT_TO_PKB}  $CFG_DT_PHR $SRC_DT_PHR $DEST_DT_PKB_PHR
${DT_TO_PKB}  $CFG_DT_PHR $SRC_DT_PHR $DEST_DT_PKB_PHR

echo ${DT_TO_PKB}  $CFG_DT_ACC $SRC_DT_ACC $DEST_DT_PKB_ACC
${DT_TO_PKB}  $CFG_DT_ACC $SRC_DT_ACC $DEST_DT_PKB_ACC

#siggen
echo ${DT_TO_PKB}  $CFG_DT_SG $SRC_DT_DUR $DEST_DT_PKB_DUR
${DT_TO_PKB}  $CFG_DT_SG $SRC_DT_DUR $DEST_DT_PKB_DUR
    
for i in $LFZ_RANGE; do
    eval echo ${DT_TO_PKB} \${CFG_DT_SG} \${SRC_DT_LFZ${i}} \${DEST_DT_PKB_LFZ${i}}
    eval ${DT_TO_PKB} \${CFG_DT_SG} \${SRC_DT_LFZ${i}} \${DEST_DT_PKB_LFZ${i}}
done
    
for i in $MGC_RANGE; do
    eval echo ${DT_TO_PKB} \${CFG_DT_SG} \${SRC_DT_MGC${i}} \${DEST_DT_PKB_MGC${i}}
    eval ${DT_TO_PKB} \${CFG_DT_SG} \${SRC_DT_MGC${i}} \${DEST_DT_PKB_MGC${i}}
done

#####  PDFS  ##############################

echo ${PDF_TO_PKB} $SRC_PDF_DUR $DEST_PDF_PKB_DUR
${PDF_TO_PKB} $SRC_PDF_DUR $DEST_PDF_PKB_DUR

echo ${PDF_TO_PKB} $SRC_PDF_LFZ $DEST_PDF_PKB_LFZ
${PDF_TO_PKB} $SRC_PDF_LFZ $DEST_PDF_PKB_LFZ

echo ${PDF_TO_PKB} $SRC_PDF_MGC $DEST_PDF_PKB_MGC
${PDF_TO_PKB} $SRC_PDF_MGC $DEST_PDF_PKB_MGC

echo ${PDF_TO_PKB} $SRC_PDF_PHS $DEST_PDF_PKB_PHS
${PDF_TO_PKB} $SRC_PDF_PHS $DEST_PDF_PKB_PHS

