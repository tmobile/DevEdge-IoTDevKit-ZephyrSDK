#!/bin/bash
git log | grep -A2 -m1 "Initial commit" | grep commit | awk NR==2'{print $2}'
