#!/bin/bash
git log | grep -A2 -m1 "Merge branch 'main' into tmo-main" | grep commit | awk '{print $2}'
