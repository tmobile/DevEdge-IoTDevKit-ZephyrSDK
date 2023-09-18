#!/bin/bash

## /*! \file merge3.sh
##  *  \brief Stage 3 of Procedure for merging zephyr upstream to tmo-main
##  *
##  *  The documentation here uses the method defined in:
##  *  https://github.com/Anvil/bash-doxygen
##  *
##  *  Copyright (c) 2023 T-Mobile USA, Inc.
##  */

cd ~/zephyrproject/zephyr

declare -a gitattributes=()

function search_array { # define a function that takes an array and an entry as arguments

  local array=("$@") # assign the arguments to a local array variable
  local entry="${array[${#array[@]}-1]}" # assign the last element of the array to a local entry variable
  unset 'array[${#array[@]}-1]' # remove the last element of the array

  for element in "${array[@]}"; do # loop over each element in the array
    if [ "$element" == "$entry" ]; then # check if the element matches the entry
      echo "Found $entry in the array" # do something if found
      return 1 # exit the function with success
    fi
  done

  echo "Did not find $entry in the array" # do something if not found
  return 0 # exit the function with failure
}

function create_diff_files {
  echo ; echo "Generate diff_files.txt with the differences between the two branches"

## /*! ~~(org) git diff --name-only <main or sha from a release> > diff_files.txt~~ */
## /*! ~~(bad) git diff --name-only zephyr-upstream-merge > diff_files.txt~~ */

## /*! tried --minimal --patience and --histogram but they were all the same */
#time git diff --minimal --name-only ../zephyr.src . > diff_files_minimal.txt
#time git diff --patience --name-only ../zephyr.src . > diff_files_patience.txt
#time git diff --histogram --name-only ../zephyr.src . > diff_files_histogram.txt

## /*! The Unix diff is significantly faster, but still misses some files. */
#time diff -qaNr . ../zephyr.src > diff_files1.txt
#sed 's/Files \.\/\(.*\) and \.\.\/[^ ]* differ/\1/' diff_files1.txt > diff_files.txt
#echo "/dev/null" >> diff_files.txt

## /*! the -R version swaps the inputs and creates the difference we need, */
  time git diff --name-only ../zephyr.src . > diff_files_f.txt
  time git diff -R --name-only ../zephyr.src . > diff_files_r.txt
  sed 's/.\/zephyr.src//g' diff_files_r.txt > diff_files_s.txt
  cat diff_files_f.txt diff_files_s.txt | sort -u > diff_files.txt

## /*! (out) real	0m8.248s */
## /*! (out) user	0m0.790s */
## /*! (out) sys	0m5.316s */

## /*! (out) real	0m8.212s */
## /*! (out) user	0m0.789s */
## /*! (out) sys	0m5.353s */
}

## /*! (org) copy merger.sh to zephyr folder and run it */
## /*! cp ~/Downloads/merger/merger.sh ~/zephyrproject/zephyr */

## /*! (org) Run the merger.sh script */
## /*! source merger.sh */

function read_git_attributes {
  echo ; echo "Read the gitattributes file"
  save_gitattributes=0
  while IFS= read -r line
  do
    if [ "$save_gitattributes" -eq 0 ];
    then
      if [ "$line" == ".gitattributes merge=ours" ];
      then
        save_gitattributes=1
        echo "Found 1st line in .gitattributes"
      fi
    fi
    if [ "$save_gitattributes" -eq 1 ];
    then
      first_field=$(echo "$line" | cut -d' ' -f1)
      gitattributes+=("$first_field")
      echo "$first_field is in gitattributes"
    fi
  done < .gitattributes
  echo "gitattributes has ${#gitattributes[@]} entries"
}

function handle_diff_files {
  echo ; echo "Read the diffs file and handle each one"
  local array=("$@") # assign the argument to a local array variable
  while IFS= read -r line
  do

    if [[ $line == "/dev/null" ]];
    then
      continue
    fi
    if [[ $line == "./.git/"* ]];
    then
      continue
    fi

    eline="${line:2}"

    if [ -f ../zephyr.src/"$eline" ]
    then
      # file exists in src
      search_array "${array[@]}" "$eline"
      local in_gitattributes=$?
      if [[ $in_gitattributes =~ ^[+-]?[0-9]+$ ]] && [ $in_gitattributes -eq 1 ]
      then
        echo "in src, in gitattributes, merge using ours:  $eline"
        #git merge-file --ours $line ../zephyr.src/$line ../zephyr.src/$line
        git difftool ../zephyr.src/"$eline" "$eline"
        #git difftool --tool=nvimdiff ../zephyr.src/"$eline" "$eline"
        echo
      else
        echo "in src, not in gitattributes, copy theirs:   $eline"
        cp -p ../zephyr.src/"$eline" "$eline"
      fi
    else
      # file does not exist in src
      search_array "${array[@]}" "$eline"
      local in_gitattributes=$?
      if [[ $in_gitattributes =~ ^[+-]?[0-9]+$ ]] && [ $in_gitattributes -eq 1 ]
      then
        echo "not in src, in gitattributes, use ours:      $eline"
      else
        echo "not in src, not in gitattributes, remove it: $eline"
        git rm "$eline"
      fi
    fi
  done < diff_files.txt
}

create_diff_files
read_git_attributes
handle_diff_files "${gitattributes[@]}"

rm diff_files*.txt

echo
echo "Call ./merge4.sh when done merging"
