#!/bin/sh

export LD_LIBRARY_PATH=$PWD/src/.libs
export GI_TYPELIB_PATH=$PWD/src
export TZ_DATA_FILE=$PWD/src/data/citiesInfo.txt
export ADMIN1_FILE=$PWD/src/data/admin1Codes.txt
export COUNTRY_FILE=$PWD/src/data/countryInfo.txt
export DATADIR=$PWD/src/data

exec python3 ./testtz.py
