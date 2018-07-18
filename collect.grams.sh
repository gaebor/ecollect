#!/usr/bin/env bash

sort_bash() {
    time LC_ALL=C sort -t $'\t' -srnk2
}

ecollect() {
    time bin/ecollect -a -b 1000000000 -l -s $'\t\r'
}

preprocess() {
    # see https://stackoverflow.com/a/42732391/3583290
    python -c "
from __future__ import print_function
import sys
def read_words():
    with open(sys.argv[1]) as f:
        while True:
            buf = f.read(2*1048576)
            if not buf:
                break

            while not str.isspace(buf[-1]):
                ch = f.read(1)
                if not ch:
                    break
                buf += ch

            words = buf.split()
            for word in words:
                yield word
ngram = []
reader = iter(read_words())
while len(ngram) < int(sys.argv[2]):
    try:
        ngram.append(next(reader))
    except:
        pass
print(*ngram)
del ngram[0]
for word in reader:
    ngram.append(word)
    print(*ngram)
    del ngram[0]
" $1 $2
}

mkdir -p bin
cd bin
cmake .. && make
if [[ $? -ne 0 ]]
then
    exit
fi
cd ..

for i in 1 2 3 4 5
do
    for file in $@
    do
        logname=`basename $file`."$i"grams
        echo $logname
        preprocess $file $i | ecollect 2>&1 > $logname.txt | tee $logname.log
        echo >> $logname.log
        sort_bash < $logname.txt 2>&1 > $logname.sort | tee -a $logname.log
    done
done
