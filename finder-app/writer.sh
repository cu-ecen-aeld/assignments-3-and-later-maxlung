#!/bin/bash
if [ "$#" -ne 2 ]
then
    echo "error. need two arguments"
    exit 1
fi

if [ -z $1 ]
then
    echo "error. need address"
    exit 1
fi

if [ -z $2 ]
then
    echo "error. need text"
    exit 1
fi
writefile=$1
writestr=$2
dir="$(dirname "${writefile}")";


if [ ! -d ${dir} ]
then
	mkdir -p ${dir}
fi

if [ ! -e $1 ]
then
	touch $1
fi


echo ${writestr} > ${writefile}

if [ $? -eq 0 ]
then 
    exit 0
else
    echo "!Error: File could not be created: ${writefile}"
    exit 1
fi




