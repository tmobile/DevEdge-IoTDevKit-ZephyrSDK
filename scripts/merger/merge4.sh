#!/bin/bash

set -eu

## /*! \file merge4.sh
##  *  \brief Stage 4 of Procedure for merging zephyr upstream to tmo-main
##  *
##  *  The documentation here uses the method defined in:
##  *  https://github.com/Anvil/bash-doxygen
##  *
##  *  Copyright (c) 2023 T-Mobile USA, Inc.
##  */

cd ~/zephyrproject/zephyr

echo "Build various things, check for issues, and correct them as needed"

## /*! (org) tmo_shell for DevEdge */
## /*! ~~west build DevEdge-IoTDevKit-ZephyrSDK/samples/tmo_shell -b tmo_dev_edge -p~~
#west build -p -b tmo_dev_edge ../tmo-zephyr-sdk/samples/tmo_shell
west build -p -b tmo_dev_edge samples/subsys/shell/shell_module

west flash

echo "Press Enter to continue..."
read -r

time west twister --device-testing \
                --hardware-map ../tmo-zephyr-sdk/maps/DaR/tmo_dev_edge-OSX-map.yml \
                --quarantine-list ../tmo-zephyr-sdk/quarantine/tmo_dev_edge-quarantine.yaml

## /*! (org) Create a PR for the changes. Done earlier */
## /*! ~~git checkout -b <branch name>~~ */

## /*! (org) Commit the changes. Done earlier ##
## /*! ~~git commit -as~~ */

echo ; echo -n "Did the tests look good? [yes] "
read -r CONTINUE
case $CONTINUE in
    y | yes | "") echo "ok" ;;
    n | no) echo "then fix the code!"
	    exit;;
    *) echo "unknown response"
       exit;;
esac

echo
echo "Push the changes"
echo "Call git push origin zephyr-downstream-merge or sdk-downstream-merge"
echo "From the repo that you are merging into"
