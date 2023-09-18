#!/bin/bash

set -eu

## /*! \file merge1.sh
##  *  \brief Stage 1 procedure for merging zephyr upstream to our main
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

echo "Start in a clean ~/zephyrproject"

##     Make sure it is backed up if needed. */
cd ~/
if test -d zephyrproject; then
    echo -n "**IS ~/zephyrproject BACKED UP**? [yes] "
    read -r CONTINUE
    case $CONTINUE in
	y | yes | "") echo "ok deleting ~/zephyrproject" ;;
	n | no) echo "then do that now!"
		exit;;
	*) echo "unknown response"
	   exit;;
    esac
fi

rm -Rf ~/zephyrproject

echo "Creating virtual python environment"
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
pip install --upgrade pip
pip install west

echo "Installing Zephyr RTOS code"
west init -m https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git --mr tmo-main zephyrproject
cd ~/zephyrproject
west update
west zephyr-export
pip install -r ~/zephyrproject/zephyr/scripts/requirements.txt
cd ~/zephyrproject/zephyr

## /*! Not following: zephyr-env.sh: openBinaryFile: does not exist (No such file or directory) */
# shellcheck disable=SC1091
source zephyr-env.sh

echo
echo "Setup remote pointers for:"
echo "    new = Zephyr's public zephyrproject-rtos/zephyr,"
echo "    origin = Our public fork of Zephyr's downstream RTOS."

git remote -v
## /*! (out) origin  https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (fetch) */
## /*! (out) origin  https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (push) */
git remote add new https://github.com/zephyrproject-rtos/zephyr.git
git remote -v
## /*! (out) new     https://github.com/zephyrproject-rtos/zephyr.git (fetch) */
## /*! (out) new     https://github.com/zephyrproject-rtos/zephyr.git (push) */
## /*! (out) origin  https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (fetch) */
## /*! (out) origin  https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (push) */
git fetch --all
## /*! (out) Fetching origin */
## /*! (out) Fetching new */

echo
git checkout -b zephyr-downstream-merge
## /*! (out) Switched to a new branch 'zephyr-downstream-merge' */

echo
echo "Clone Zephyr SDK for testing later"

cd ~/zephyrproject
git clone https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrSDK.git tmo-zephyr-sdk

cd tmo-zephyr-sdk

echo
git checkout -b sdk-downstream-merge
## /*! (out) Switched to a new branch 'sdk-downstream-merge' */

echo
echo "Clone zephyr to folder zephyr.src"

cd ~/zephyrproject
git clone https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git zephyr.src

## /*! (org) Checkout the "source" branch
##     (could be main, or could be a sha from a release) */
## /*! (org) The ones here are for 3.4.0-rc2 or main */

cd zephyr.src
## ~~(org) git checkout 2ad1a24fd60d0df8cb45fb6ed6acf7b0d3820754~~ NOT TESTED YET
git checkout main

echo
echo "Setup remote pointers for:"
echo "    new = Zephyr's public zephyrproject-rtos/zephyr,"
echo "    origin = Our public fork of Zephyr's downstream RTOS."

git remote -v
## /*! (out) origin  https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (fetch) */
## /*! (out) origin  https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (push) */
git remote add new https://github.com/zephyrproject-rtos/zephyr.git
git remote -v
## /*! (out) new     https://github.com/zephyrproject-rtos/zephyr.git (fetch) */
## /*! (out) new     https://github.com/zephyrproject-rtos/zephyr.git (push) */
## /*! (out) origin  https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (fetch) */
## /*! (out) origin  https://github.com/tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (push) */
git fetch --all
## /*! (out) Fetching origin */
## /*! (out) Fetching new */

echo
git checkout -b zephyr-upstream-merge
## /*! (out) Switched to a new branch 'zephyr-upstream-merge' */

echo
echo "Call ./merge3.sh to start merge from (upstream) main to (downstream) tmo-main"
