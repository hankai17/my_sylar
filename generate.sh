#!/bin/sh

command_error_exit() {
    $*
    if [ $? -ne 0 ]
    then
        exit 1
    fi
}

if [ $# -lt 2 ]
then
    echo "use [$0 project_name namespace]"
    exit 0
fi

project_name=$1
namespace=$2

command_error_exit mkdir $project_name
command_error_exit cd $project_name
command_error_exit git clone https://github.com/hankai17/my_sylar/my_sylar.git




