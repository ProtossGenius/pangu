echo "\033[32m ##########   TEST ANALYSIS LEXER #########\033[0m"; \

for i in `ls test_datas/lexer/* | grep -v -E '.out$|.target$'`; do \
    echo "\033[32m analysising lexer $i ......\033[0m"; \
    ./build/pangu-lexer $i > $i.out; \
    diff $i.target  $i.out; \
    if [ $? -ne 0 ]; then echo "\033[31m lexer $i fail \033[0m"; fi
done
