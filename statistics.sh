#!/bin/bash

echo "Total: "
git ls-files *.cpp *.h | xargs wc -l | grep "total"

cd core
dirs=`ls -d */`
for d in $dirs
do
    echo -e "$d:  \c"
    git ls-files *.cpp *.h | grep "$d" | xargs wc -l | grep "total"
done
