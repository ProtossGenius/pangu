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

test_compile_fail () {
    echo "\033[32m ########## TEST COMPILE FAIL #########\033[0m";
    for i in `ls test_datas/compile_fail/* | grep -v -E '.out$|.error$'`; do
        echo "\033[32m compilering failure case $i ......\033[0m";
        ./build/pangu compile $i > $i.out 2>&1;
        if [ $? -eq 0 ]; then echo "\033[31m expected compile failure for $i \033[0m"; exit; fi
        grep -F "`cat $i.error`" $i.out > /dev/null;
        if [ $? -ne 0 ]; then echo "\033[31m compile fail output mismatch for $i \033[0m"; cat $i.out; exit; fi
    done
}

test lexer 
test grammer 
test runtime
test_compile
test_compile_fail
