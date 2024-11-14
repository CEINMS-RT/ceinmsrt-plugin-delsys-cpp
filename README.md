# Delsys Plugin

## Installation
* Install Delsys SDK (Tested version: 2.6.12) (download package 'EMGWorks': https://delsys.com/support/software/)
* Make sure you can open the software 'Trigno Control Utility'. If not look at the troubleshooting section at the end of the file. 
* Compile, generate and build the EMG_Delsys plugin with VS 2019 (Compile it after compiling CEINMS). It can happen that when building the plugin the boost libraries are not found. Check in the Cmake guy that you are using the right Boost_DIR and Boost_INCLUDE_DIR. If not just change those variables with the right path. Then compile, generate and build again. 
 
## How to use it
* Plug the Delsys Trigno station into the power source and plug the USB from the station into the computer
* Open Delsys Trigno Control Utility and turn on the EMG sensor(s)
* Check if all sensors have a blinking green light and if you see them on from the interface of Trigno Control Utility
* Push the button 'Start Analog'
* Before running CEINMS you will need to set a few XML files:
    * ExecutionRT.xml → change the <EMGDevice> value to the path and file name of the right dll: pathTo\EMG_Delsys.dll;
    * ExecutionEMG.xml → make sure you have the right IP number and that you have the same number of <maxEMG\> values as the number of the sensor you are using
    * subjectMTUUncalibrate.xml → at the bottom of the file in the <Channels\> list, make sure you have a good mapping EMG channel-to-MTU. List the channel in the same order as the sensors. Sensor 1 has to be the first Channel in the list with <name> as you prefer (e.g. emg01 or nameMuscle) and <muscleSequence\> as the name of the MTU (you must use OpenSim/CEINMS naming convention) to be fed by that EMG sensor. 
* Once the CEINMS files are set you can run CEINMS
* If the recording is enabled, the EMG signals can be found in the output folder

## Troubleshooting

If the 'Trigno Control Utility' software does not work, you can follow these steps to solve the issue:
* firstly open up the "Device Managers" window 
* N"USBXPress Device" and uninstalling the device (select the option of deleting the driver);
* Unplug the USB cable from the Base Station and left it out for a few moments. 
* Then plug in the USB cable to the computer. Typically the "USBXpress Device" should repopulate by itself.
