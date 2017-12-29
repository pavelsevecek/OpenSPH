#!/bin/bash

echo "Total: "
git ls-files | xargs wc -l | grep "total"

cd lib
dirs=`ls -d */`
for d in $dirs
do
    echo -e "$d:  \c"
    git ls-files | grep "$d" | xargs wc -l | grep "total"
done
