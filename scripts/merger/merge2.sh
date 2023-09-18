#!/bin/bash

set -eu

## /*! \file merge2.sh
##  *  \brief Stage 2 of Procedure for merging zephyr upstream to our main
##  *
##  * This is now legacy and replaced by: .github/workflows/tmo-mirror.yml
##  *
##  *  The downstream merging method comes from:
##  *  https://stackoverflow.com/questions/37471740/
##  *  how-to-copy-commits-from-one-git-repo-to-another
##  *  answered by WiR3D on Nov 13, 2020 at 13:39
##  *
##  *  The documentation here uses the method defined in:
##  *  https://github.com/Anvil/bash-doxygen
##  *
##  *  Copyright (c) 2023 T-Mobile USA, Inc.
##  */

cd ~/zephyrproject/zephyr.src

## /*! (org) Run the (manual/auto) merge */

## /*! (org) git merge -X theirs --no-edit --allow-unrelated-histories
##           <main or sha from a release> */

echo "Remember the sig of the 1st and last entries"
echo "Only a limited number of changed files can be diff'ed"
echo "So go to the last page 1st and choose what your system can handle"
echo "Press Enter to continue..."
read -r

git log --graph --oneline --pretty=format:"%h%x09%an%x09%ad%x09%s" \
        --abbrev-commit --date=relative remotes/origin/main..remotes/new/main
echo "Enter first entry: "
read -r FIRST
echo "Enter last entry: "
read -r LAST
git cherry-pick --allow-empty --strategy recursive --strategy-option theirs "$LAST"^.."$FIRST"

echo
echo "You may have to manually finish the cherry-pick with these commands"
echo "git cherry-pick --allow-empty --strategy recursive --strategy-option theirs"
echo "git cherry-pick --allow-empty --continue"
echo "git commit --allow-empty"
echo "Call ./merge3.sh when done cherry-picking"
