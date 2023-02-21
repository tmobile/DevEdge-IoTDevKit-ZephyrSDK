.. _letimer:

LETIMER
###########

Overview
********

A simple sample that can be used with any silabs gecko based board and
tests the LETIMER driver

Building and Running
********************

This application can be built and executed on DevEdge as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/letimer
   :host-os: unix
   :board: tmo_dev_edge
   :goals: run
   :compact:

To build for another board, change "tmo_dev_edge" above to that board's name.

Sample Output
=============

.. code-block:: console

    1 second
    1 second (2)
    1 second (3)
    1 second (4)
    1 second (5)
    60 second
