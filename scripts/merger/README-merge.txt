Start with a command window where you backed up a local copy of scripts/merger folder
and call ./merge1.sh

Start in a clean ~/zephyrproject
**IS ~/zephyrproject BACKED UP**? [yes] ok
rm -Rf ~/zephyrproject

west init -m git@github.com:tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git --mr tmo-main zephyrproject
=== Initializing in /home/druffer/zephyrproject
--- Cloning manifest repository from git@github.com:tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git, rev. tmo-main
--- setting manifest.path to zephyr
=== Initialized. Now run "west update" inside /home/druffer/zephyrproject.

cd ~/zephyrproject
west update

Much output follows from the west update...

west zephyr-export

west zephyr-export output:

Zephyr (/home/druffer/zephyrproject/zephyr/share/zephyr-package/cmake)
has been added to the user package registry in:
~/.cmake/packages/Zephyr

ZephyrUnittest (/home/druffer/zephyrproject/zephyr/share/zephyrunittest-package/cmake)
has been added to the user package registry in:
~/.cmake/packages/ZephyrUnittest

cd ~/zephyrproject/zephyr
source zephyr-env.sh

Clone Zephyr SDK for testing later

cd ~/zephyrproject
git clone git@github.com:tmobile/DevEdge-IoTDevKit-ZephyrSDK.git tmo-zephyr-sdk

Clone zephyr to folder zephyr.src

cd ~/zephyrproject
git clone git@github.com:tmobile/DevEdge-IoTDevKit-ZephyrRTOS zephyr.src

cd zephyr.src
git checkout main

cd ../zephyr

Setup remote pointers for:
    new = Zephyr's public zephyrproject-rtos/zephyr
    origin = Our public repo of our public downstream RTOS.
origin	git@github.com:tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (fetch)
origin	git@github.com:tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (push)
new	git@github.com:zephyrproject-rtos/zephyr.git (fetch)
new	git@github.com:zephyrproject-rtos/zephyr.git (push)
origin	git@github.com:tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (fetch)
origin	git@github.com:tmobile/DevEdge-IoTDevKit-ZephyrRTOS.git (push)
Fetching origin
Fetching new

Switched to a new branch 'zephyr-upstream-merge'

Call ./merge2.sh to start cherry-picking

Remember the sig of the 1st and last entries
Only a limited number of changed files can be diff'ed
So go to the last page 1st and choose what your system can handle
Press Enter to continue...

* 3a24476fb7    Anas Nashif     33 hours ago    tests: kernel: fix some test identifiers
* aa4f24694e    Peter Mitsis    6 weeks ago     test: Enhance benchmark latency reporting
* fe922d30f9    Peter Mitsis    6 weeks ago     test: refactor benchmark latency PRINT_F() macro
* 5c93b92a77    Peter Mitsis    6 weeks ago     test: benchmark latency macro parameter names
* ebb1fa585f    Fabio Baltieri  7 days ago      dma: iproc_pax_v2: delay initialization after pcie
* 20e7c6db6c    Fabio Baltieri  12 days ago     video: mcux_csi: set a dedicated init priority for video_mcux_csi
* 7839eb524c    Mike J. Chen    3 weeks ago     drivers: dma: dma_lpc: fix bug with transfer size/width
* 04f488accf    Mike J. Chen    2 days ago      drivers: spi: mcux_flexcomm: fix DMA bug for 2-byte transfers
* f882d31ea7    Mike J. Chen    3 weeks ago     drivers: i2s: mcux_flexcomm: fix multiple bugs
* 0d33ecd56a    Benedikt Schmidt        3 months ago    drivers: adc: configurable wait for completion timeout
* feef931fbb    Erwan Gouriou   10 hours ago    drivers: counter: stm32: Use const TIM_TypeDef on stm32f2 series
* d53bbffb71    Christopher Friedt      28 hours ago    MAINTAINERS: add cfriedt as kernel collaborator
* b5f8c7154d    Christopher Friedt      12 hours ago    tests: posix: barrier: use consistent test names
* 797a9afd89    Christopher Friedt      28 hours ago    MAINTAINERS: remove cfriedt as GPIO collaborator
* e5fcca6e99    Stine Åkredalen 16 hours ago    Bluetooth: mesh: Update default values for transport SAR configuration
* bc0eb32a5e    Benjamin Cabé   2 days ago      pcie: doc: Fix doxygen doc for PCIe capabilities
* b7ec7ec638    Anas Nashif     2 days ago      debug: thread_analyzer: use printk by default
* f6f3eed7a0    Benjamin Cabé   32 hours ago    bluetooth: ead: doc: Add missing Doxygen header
* 53b93829a3    Benjamin Cabé   32 hours ago    bluetooth: ead: Add include guards
* 1c0c2a095b    Serhiy Katsyuba 5 weeks ago     drivers: intel_adsp_gpdma: Fix release ownership
* 6254527343    Francois Ramu   2 days ago      drivers: timer: stm32 lptim driver check clock_control_on return code
* e13c193acf    Erwan Gouriou   5 months ago    boards: nucleo_wba52cg: Update core freq to provide 48MHz on PLLQ
* ef0d358048    Erwan Gouriou   5 months ago    dts: stm32wba: Add RNG node
* 3fba82490b    Guillaume Gautier       4 weeks ago     dts: arm: st: update stm32f1 and f3 dtsi with new rcc bindings
* 208d962eb8    Guillaume Gautier       4 weeks ago     drivers: clock_control: stm32 set adc prescaler in rcc
* 5a55a185dd    Guillaume Gautier       4 weeks ago     dts: bindings: clock: add specific rcc bindings for stm32f1x and f3x
* 45f4f271d2    Marc Desvaux    12 days ago     dts: arm: st: h5: add Ethernet
* 6e2cad555a    Marc Desvaux    12 days ago     board: arm: stm32h573i_dk: add Ethernet
* 09da4cf89d    Martin Kiepfer  7 weeks ago     driver: regulator: Add support for AXP192 power management IC
* 820bc9267e    Marcin Zapolski 2 days ago      drivers: flash: stm32l4: Fix STM32L4Q5 support in flash driver
* 6d824667df    Marc Desvaux    33 hours ago    modules: align Kconfig.stm32
:

Copy the 1st entry (3a24476fb7)

... Page through multiple pages untill the last

* 8db0349bec    Anas Nashif     5 weeks ago     doc: release: add one entry re ztest new API
* 7749607343    Anas Nashif     5 weeks ago     doc: release: remove empty sections
* 6015293447    Anas Nashif     5 weeks ago     doc: release: Use past tense on some entries
* 288fd5d3f2    Anas Nashif     5 weeks ago     doc: release: adapt title of 3.4 release notes
* beae29964f    Evgeniy Paltsev 5 weeks ago     doc: ARC: release notes: typo fix
* f8803090ca    Evgeniy Paltsev 5 weeks ago     dos: ARC: mark DSP AGU/XY extensions as supported on ARC EM
* e86185522c    Carles Cufi     5 weeks ago     Bluetooth: release notes: Fix formatting of indented bullet points
* 052c23b922    Evgeniy Paltsev 5 weeks ago     doc: ARC: add release notes for ARC
* 7d89f784a4    Andrzej Głąbek  5 weeks ago     doc: release: Add v3.4 notes for ADC and PWM and Nordic related stuff
* 14200f82db    Théo Battrel    5 weeks ago     Bluetooth: Tests: New bsim test for ID
* 363676764a    Théo Battrel    5 weeks ago     Bluetooth: Host: Fix wrong ID being stored
* 09085ef63c    Jaska Uimonen   5 weeks ago     dts: xtensa: intel: update cavs25 sram size
* b2c00ec032    Mingjie Shen    5 weeks ago     net: utils: fix offset used before range check
* 733a35864a    Divin Raj       6 weeks ago     drivers: ethernet: Fix typo in comment
* 3822870d28    Thomas Stranger 5 weeks ago     drivers: mdio: adin2111: correct prompt
* 68dc53b077    Flavio Ceolin   5 weeks ago     doc: release-notes: PM related release notes
* b7f35a8f29    Flavio Ceolin   5 weeks ago     doc: vulnerabilities: Add information about new vulnerabilities
* f683b0f35e    Flavio Ceolin   5 weeks ago     doc: release-notes: Security related release-notes-3
* 785d9bdc67    Aedan Cullen    6 weeks ago     drivers: display: fix zero-buffers-in-SRAM case in DCNANO LCDIF
* c59b57c0be    Gerard Marull-Paretas   5 weeks ago     soc: esp32*: do not enable HAS_DYNAMIC_DEVICE_HANDLES
* 721e4aa8b3    Anas Nashif     5 weeks ago     release: fix layout/typos in release notes
* 644d02480e    Johann Fischer  5 weeks ago     doc: release-notes-3.4: add release notes for USB and display
* c9f8b8b78a    Thomas Stranger 5 weeks ago     drivers: sensor: shtcx: fix val2 calculation
* ecf2cb5932    Maxim Adelman   5 weeks ago     kernel shell, stacks shell commands: iterate unlocked on SMP
* fbb6cd1a39    Johann Fischer  5 weeks ago     doc: connectivity: move USB-C device stack to USB chapter
* b335c19bcb    Johann Fischer  6 weeks ago     doc: move USB documentation to connectivity
* 09a9a7edf4    Benjamin Cabé   5 weeks ago     doc: release-notes: fix typo with i.MX93 board
* 4bc46e4e3a    Benjamin Cabé   5 weeks ago     doc: release-notes: sort ARM boards alphabetically
* 8963b50716    Benjamin Cabé   5 weeks ago     doc: release-notes: add missing boards
* 83b074c418    Jamie McCrae    5 weeks ago     doc: release: 3.4: Add build system relative path fixes
* 9642c48a29    Jamie McCrae    5 weeks ago     cmake: boards: Fix issue with relative paths
(END)

Hit q to end the list
Enter first entry:
3a24476fb7
Enter last entry: 
9642c48a29

git cherry-pick --allow-empty --strategy recursive --strategy-option theirs "$LAST"^.."$FIRST"

Frequently, you will be creating a comment that has no files, which, in this case, is true because
we already have this commit in both repositories, but we still need the commit so we don't get
asked about it again.

The previous cherry-pick is now empty, possibly due to conflict resolution.
If you wish to commit it anyway, use:

    git commit --allow-empty

and then use:

    git cherry-pick --continue

to resume cherry-picking the remaining commits.
If you wish to skip this commit, use:

    git cherry-pick --skip

On branch rtos-private-pull
Cherry-pick currently in progress.
  (run "git cherry-pick --continue" to continue)
  (use "git cherry-pick --skip" to skip this patch)
  (use "git cherry-pick --abort" to cancel the cherry-pick operation)

nothing to commit, working tree clean

The script ends here and you need to do those commands in the ~/zephyrproject/zephyr window

To get the command window caught up with the cherry-pick, type: git cherry-pick --allow-empty --continue
Then you can follow the instructions and type: git commit --allow-empty

An editor window will open and you can simply type :w and the :q depending on the editor you are using

Once closed, the commit will continue, as it would have been if no error was detected

You may have to manually finish the cherry-pick with these commands
git cherry-pick --allow-empty --strategy recursive --strategy-option theirs
git cherry-pick --allow-empty --continue
git commit --allow-empty
Call ./merge3.sh when done cherry-picking

Generate diff_file.txt with the files that are different between the two branches

real	0m26.121s
user	0m10.301s
sys	0m7.197s

Read the gitattributes file
Found 1st line in .gitattributes
.gitattributes is in gitattributes

Many lines of output follow, creating the gitattributes array

Read the diffs file and handle each one
.gitattributes found in gitattributes
in src, in gitattributes, merge using ours:  .gitattributes

We can't do merges in a pull request, so we have to merge manually with:
git difftool remotes/origin/main "$eline"
Which outputs:

Viewing (1/1): '.gitattributes'
Launch 'bc3' [Y/n]? 

And then will display a side by side merge screen.
I use BeyondCompare and you will find a merge3.png file with this file.
You can find instructions for installing this tool at: https://www.scootersoftware.com/kb/vcs

Other output lines are:

in src, not in gitattributes, copy theirs:   MAINTAINERS.yml
not in src, in gitattributes, use ours:      boards/arm/tmo_dev_edge/CMakeLists.txt
not in src, not in gitattributes, remove it: doc/connectivity/usb/api/hid.rst

Call ./merge2.sh to do more cherry-picking
Call ./merge4.sh when done cherry-picking

Finally, you have to test the features that you have just put into your sources by
calling ./merge4.sh and it will test for you.

west build -p -b tmo_dev_edge ../tmo-zephyr-sdk/samples/tmo_shell -- -G"Eclipse CDT4 - Ninja"

Build various things, check for issues, and correct them as needed
-- west build: making build dir /home/druffer/zephyrproject/zephyr/build pristine
-- west build: generating a build system
-- Application: /home/druffer/zephyrproject/tmo-zephyr-sdk/samples/tmo_shell
-- CMake version: 3.26.4
-- Found Python3: /usr/bin/python3.8 (found suitable exact version "3.8.10") found components: Interpreter 
-- Cache files will be written to: /home/druffer/.cache/zephyr
-- Zephyr version: 3.4.99 (/home/druffer/zephyrproject/zephyr)
-- Found west (found suitable version "1.0.0", minimum required is "0.14.0")
-- Board: tmo_dev_edge
-- ZEPHYR_TOOLCHAIN_VARIANT not set, trying to locate Zephyr SDK
-- Found host-tools: zephyr 0.16.1 (/home/druffer/zephyr-sdk-0.16.1)
-- Found toolchain: zephyr 0.16.1 (/home/druffer/zephyr-sdk-0.16.1)
-- Found Dtc: /home/druffer/zephyr-sdk-0.16.1/sysroots/x86_64-pokysdk-linux/usr/bin/dtc (found suitable version "1.6.0", minimum required is "1.4.6") 
-- Found BOARD.dts: /home/druffer/zephyrproject/zephyr/boards/arm/tmo_dev_edge/tmo_dev_edge.dts
-- Found devicetree overlay: /home/druffer/zephyrproject/tmo-zephyr-sdk/samples/tmo_shell/boards/tmo_dev_edge.overlay
-- Generated zephyr.dts: /home/druffer/zephyrproject/zephyr/build/zephyr/zephyr.dts
-- Generated devicetree_generated.h: /home/druffer/zephyrproject/zephyr/build/zephyr/include/generated/devicetree_generated.h
-- Including generated dts.cmake file: /home/druffer/zephyrproject/zephyr/build/zephyr/dts.cmake
Parsing /home/druffer/zephyrproject/tmo-zephyr-sdk/samples/tmo_shell/Kconfig
Loaded configuration '/home/druffer/zephyrproject/zephyr/boards/arm/tmo_dev_edge/tmo_dev_edge_defconfig'
Merged configuration '/home/druffer/zephyrproject/tmo-zephyr-sdk/samples/tmo_shell/prj.conf'
Merged configuration '/home/druffer/zephyrproject/tmo-zephyr-sdk/samples/tmo_shell/boards/tmo_dev_edge.conf'
Configuration saved to '/home/druffer/zephyrproject/zephyr/build/zephyr/.config'
Kconfig header saved to '/home/druffer/zephyrproject/zephyr/build/zephyr/include/generated/autoconf.h'
-- Found GnuLd: /home/druffer/zephyr-sdk-0.16.1/arm-zephyr-eabi/bin/../lib/gcc/arm-zephyr-eabi/12.2.0/../../../../arm-zephyr-eabi/bin/ld.bfd (found version "2.38") 
-- The C compiler identification is GNU 12.2.0
-- The CXX compiler identification is GNU 12.2.0
-- Could not determine Eclipse version, assuming at least 3.6 (Helios). Adjust CMAKE_ECLIPSE_VERSION if this is wrong.
-- The ASM compiler identification is GNU
-- Found assembler: /home/druffer/zephyr-sdk-0.16.1/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc
-- Configuring done (6.8s)
-- Generating done (0.3s)
-- Build files have been written to: /home/druffer/zephyrproject/zephyr/build
-- west build: building application
[1/515] Generating zephyr/include/generated/servercert.der.inc
[2/515] Generating zephyr/include/generated/entrust_g2_ca.der.inc

...

[502/515] Building C object zephyr/kernel/CMakeFiles/kernel.dir/mempool.c.obj
[503/515] Building C object zephyr/kernel/CMakeFiles/kernel.dir/poll.c.obj
[504/515] Linking C static library zephyr/kernel/libkernel.a
[505/515] Linking C executable zephyr/zephyr_pre0.elf

[506/515] Generating dev_handles.c
[507/515] Building C object zephyr/CMakeFiles/zephyr_pre1.dir/misc/empty_file.c.obj
[508/515] Building C object zephyr/CMakeFiles/zephyr_pre1.dir/dev_handles.c.obj
[509/515] Linking C executable zephyr/zephyr_pre1.elf

[510/515] Generating linker.cmd
[511/515] Generating isr_tables.c, isrList.bin
[512/515] Building C object zephyr/CMakeFiles/zephyr_final.dir/misc/empty_file.c.obj
[513/515] Building C object zephyr/CMakeFiles/zephyr_final.dir/isr_tables.c.obj
[514/515] Building C object zephyr/CMakeFiles/zephyr_final.dir/dev_handles.c.obj
[515/515] Linking C executable zephyr/zephyr.elf
Memory region         Used Size  Region Size  %age Used
           FLASH:      441564 B         1 MB     42.11%
             RAM:      180668 B       256 KB     68.92%
        IDT_LIST:          0 GB         2 KB      0.00%
-- west flash: rebuilding
ninja: no work to do.
-- west flash: using runner jlink
-- runners.jlink: JLink version: 7.82
-- runners.jlink: Flashing file: /home/druffer/zephyrproject/zephyr/build/zephyr/zephyr.hex
Press Enter to continue...

scripts/twister -v --no-clean --enable-slow --device-testing \
                --hardware-map ../tmo-zephyr-sdk/maps/DaR/tmo_dev_edge-map.yml \
                --quarantine-list ../tmo-zephyr-sdk/quarantine/tmo_dev_edge-quarantine.yaml

ZEPHYR_BASE unset, using "/home/druffer/zephyrproject/zephyr"
Keeping artifacts untouched
INFO    - Using Ninja..
INFO    - Zephyr version: catchup-to-Zephyr-3.4.0-release-125-ga3d00c340d82
INFO    - Using 'zephyr' toolchain.
INFO    - Building initial testsuite list...

Device testing on:

| Platform     |           ID | Serial device   |
|--------------|--------------|-----------------|
| tmo_dev_edge | 000900032606 | /dev/ttyACM1    |

INFO    - JOBS: 1
INFO    - Adding tasks to the queue...
INFO    - Added initial list of jobs to queue
INFO    - 1662/2122 tmo_dev_edge              samples/sensor/esp32_temp_sensor/sample.sensor.esp32_temp_sensor SKIPPED (runtime filter)
INFO    - 1663/2122 tmo_dev_edge              samples/sensor/lsm6dso/sample.sensor.lsm6dso       SKIPPED (runtime filter)

...

INFO    - 2121/2122 tmo_dev_edge              tests/ztest/base/testing.ztest.base.verbose_0_userspace PASSED (device 11.505s)
INFO    - 2122/2122 tmo_dev_edge              tests/ztest/base/testing.ztest.base.verbose_0      PASSED (device 9.799s)

INFO    - 2332 test scenarios (2122 test instances) selected, 1713 configurations skipped (1661 by static filter, 52 at runtime).
INFO    - 409 of 2122 test configurations passed (100.00%), 0 failed, 0 errored, 1713 skipped with 0 warnings in 13958.26 seconds
INFO    - In total 4117 test cases were executed, 6869 skipped on 1 out of total 574 platforms (0.17%)
INFO    - 409 test configurations executed on platforms, 0 test configurations were only built.

Hardware distribution summary:

| Board        |           ID |   Counter |
|--------------|--------------|-----------|
| tmo_dev_edge | 000900032606 |       409 |
INFO    - Saving reports...
INFO    - Writing JSON report /home/druffer/zephyrproject/zephyr/twister-out/twister.json
INFO    - Writing xunit report /home/druffer/zephyrproject/zephyr/twister-out/twister.xml...
INFO    - Writing xunit report /home/druffer/zephyrproject/zephyr/twister-out/twister_report.xml...
INFO    - Run completed

Did the tests look good? [yes]

echo "Pushing the changes"
git push origin zephyr-upstream-merge
