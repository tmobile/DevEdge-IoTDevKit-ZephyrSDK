#!/bin/bash
git log --grep "Merge branch 'main' into tmo-main" --max-count 1 --pretty="format:%H"
