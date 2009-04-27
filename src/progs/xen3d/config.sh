#!/bin/bash

usage=

if [ $# -lt 1 ]
then
    echo "Not enough arguments"
    usage=1
fi

if [[ $1 != "x86" && $1 != "x86-64" ]]
then
    echo "Wrong argument"
    usage=1
fi

if [[ $usage == 1 ]]
then
    echo "Usage: config.sh arch"
    echo "arch must be x86 or x86-64"
    echo "arg1 was $1"
    exit 1
fi

if [ $1 == "x86" ]
then
    echo "Configuring for x86"
    echo "MESA_LIB_DIR = lib" > ./Makefile.config
    echo "mesadir=\"lib\"" > ./run.config
    echo "GALLIUM_BUILD_DIR = build/linux-x86" >> ./Makefile.config
    echo "galdir=\"build/linux-x86\"" >> ./run.config
    exit 0
fi

if [ $1 == "x86-64" ]
then
    echo "Configuring for x86-64"
    echo "MESA_LIB_DIR = lib64" > ./Makefile.config
    echo "mesadir=\"lib64\"" > ./run.config
    echo "GALLIUM_BUILD_DIR = build/linux-x86_64" >> ./Makefile.config
    echo "galdir=\"build/linux-x86_64\"" >> ./run.config
    exit 0
fi
