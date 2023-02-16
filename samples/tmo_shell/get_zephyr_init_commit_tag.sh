#!/bin/bash
git log --grep "^Initial commit$" --max-count 1 --pretty="format:%H"
