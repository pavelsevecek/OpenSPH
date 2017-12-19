#!/bin/bash

git ls-files | xargs wc -l | grep "total"

git ls-files | grep -v "test" | xargs wc -l | grep "total"
