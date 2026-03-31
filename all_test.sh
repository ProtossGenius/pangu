#!/usr/bin/bash
test () {
    echo "\033[32m ########## TEST `echo $1|tr a-z A-Z` #########\033[0m"; 
    for i in `ls test_datas/$1/* | grep -v -E '.out$|.target$'`; do 
        echo "\033[32m analysising $1 $i ......\033[0m"; 
        if [ "$1" = "runtime" ]; then
            ./build/pangu run $i > $i.out;
        else
            ./build/pangu-$1 $i > $i.out;
        fi
        if [ $? -eq 0 ]; then diff $i.target  $i.out; else exit; fi 
        if [ $? -ne 0 ]; then echo "\033[31m test $1 $i fail \033[0m"; exit; fi 
    done
}

test_compile () {
    echo "\033[32m ########## TEST COMPILE #########\033[0m";
    for i in `ls test_datas/compile/* | grep -v -E '.out$|.target$'`; do
        echo "\033[32m compilering $i ......\033[0m";
        local bin=./build/`basename $i .pgl`
        ./build/pangu compile $i > /tmp/pangu-compile-path.out;
        if [ $? -ne 0 ]; then exit; fi
        $bin > $i.out;
        if [ $? -eq 0 ]; then diff $i.target $i.out; else exit; fi
        if [ $? -ne 0 ]; then echo "\033[31m test compile $i fail \033[0m"; exit; fi
        rm -f $bin $bin.ll
    done
}

test lexer 
test grammer 
test runtime
test_compile
