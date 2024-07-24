echo "\033[32m ##########   TEST ANALYSIS LEXER #########\033[0m"; 

for i in `ls test_datas/lexer/* | grep -v -E '.out$|.target$'`; do 
    echo "\033[32m analysising lexer $i ......\033[0m"; 
    ./build/pangu-lexer $i > $i.out; 
    if [ $? -eq 0 ]; then diff $i.target  $i.out; else exit; fi 
    if [ $? -ne 0 ]; then echo "\033[31m lexer $i fail \033[0m"; exit; fi 
done


echo "\033[32m ##########   TEST ANALYSIS GRAMMER #########\033[0m"; 

for i in `ls test_datas/grammer/* | grep -v -E '.out$|.target$'`; do 
    echo "\033[32m analysising grammer $i ......\033[0m"; 
    ./build/pangu-grammer $i > $i.out; 
    if [ $? -eq 0 ]; then diff $i.target  $i.out; else exit; fi 
    if [ $? -ne 0 ]; then echo "\033[31m grammer $i fail \033[0m"; exit; fi 
done

echo "\033[32m ########## ALL SUCCESS #########\033[0m"; 