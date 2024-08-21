#!/usr/bin/bash
test () {
    echo "\033[32m ########## TEST `echo $1|tr a-z A-Z` #########\033[0m"; 
    for i in `ls test_datas/$1/* | grep -v -E '.out$|.target$'`; do 
        echo "\033[32m analysising $1 $i ......\033[0m"; 
        ./build/pangu-$1 $i > $i.out; 
        if [ $? -eq 0 ]; then diff $i.target  $i.out; else exit; fi 
        if [ $? -ne 0 ]; then echo "\033[31m test $1 $i fail \033[0m"; exit; fi 
    done
}

test lexer 
test grammer 