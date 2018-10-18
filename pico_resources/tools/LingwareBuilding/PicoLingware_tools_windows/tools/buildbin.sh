#!/bin/sh
#############################################################################
# bash script buildbin.sh --- builds textana and siggen lingware resources
#                             from pkbs
#
# Copyright (C) 2009 SVOX AG. All rights reserved.
#############################################################################

## version suffixes 
VERSION_SUFFIX_it_IT_cm0="1.0.0.3-0-0"
VERSION_SUFFIX_es_ES_zl0="1.0.0.3-0-0"
VERSION_SUFFIX_de_DE_gl0="1.0.0.3-0-1"
VERSION_SUFFIX_en_GB_kh0="1.0.0.3-0-0"
VERSION_SUFFIX_en_US_lh0="1.0.0.3-0-1"
VERSION_SUFFIX_fr_FR_nk0="1.0.0.3-0-2"

TOOLSDIR=./
CONFIGSDIR=../configs/
GLW=genlingware.pl

if [ -n "$3" -o -z "$2" ]; then
    echo;
    echo "usage: $0  <langcountryid>  <speakerid>";
    echo "  e.g: $0 en-GB kh0";
    exit;
fi

# check if language supported
if [ $1 = "en-GB" ]; then
    if [ $2 = "kh0" ]; then
    	VERSION_SUFFIX="${VERSION_SUFFIX_en_GB_kh0}"
        echo
    else
        echo;
        echo invalid speaker id;
        exit;
    fi
elif [ $1 = "en-US" ]; then
    if [ $2 = "mh5" ]; then
    	VERSION_SUFFIX="${VERSION_SUFFIX_en_US_mh5}"
        echo
    elif [ $2 = "kr0" ]; then
    	VERSION_SUFFIX="${VERSION_SUFFIX_en_US_kr0}"
        echo
    elif [ $2 = "lh0" ]; then
    	VERSION_SUFFIX="${VERSION_SUFFIX_en_US_lh0}"
        echo
    else
        echo;
        echo invalid speaker id;
        exit;
    fi
elif [ $1 = "fr-FR" ]; then
    if [ $2 = "nk0" ]; then
    	VERSION_SUFFIX="${VERSION_SUFFIX_fr_FR_nk0}"
        echo
    else
        echo;
        echo invalid speaker id;
        exit;
    fi
elif [ $1 = "es-ES" ]; then
    if [ $2 = "zl0" ]; then
    	VERSION_SUFFIX="${VERSION_SUFFIX_es_ES_zl0}"
        echo
    else
        echo;
        echo invalid speaker id;
        exit;
    fi
elif [ $1 = "it-IT" ]; then
    if [ $2 = "cm0" ]; then
    	VERSION_SUFFIX="${VERSION_SUFFIX_it_IT_cm0}"
        echo
    else
        echo;
        echo invalid speaker id;
        exit;
    fi
elif [ $1 = "de-DE" ]; then
    if [ $2 = "gl0" ]; then
    	VERSION_SUFFIX="${VERSION_SUFFIX_de_DE_gl0}"
        echo
    else
        echo;
        echo invalid speaker id;
        exit;
    fi
else
    echo;
    echo invalid langcountry id;
    exit;
fi

LANG=$1
SID=$2

perl  ${TOOLSDIR}/${GLW} ${CONFIGSDIR}/${LANG}/${LANG}_ta.txt ${LANG}_ta_${VERSION_SUFFIX}.bin
perl  ${TOOLSDIR}/${GLW} ${CONFIGSDIR}/${LANG}/${LANG}_${SID}_sg.txt ${LANG}_${SID}_sg_${VERSION_SUFFIX}.bin
perl  ${TOOLSDIR}/${GLW} ${CONFIGSDIR}/${LANG}/${LANG}_dbg.txt ${LANG}_dbg.bin
