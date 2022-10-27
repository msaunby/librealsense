# Installing in Docker container

Notes by Michael Saunby. October 2022.

```sh
sudo docker run -ti -v '/dev/bus/usb':'/dev/bus/usb' --privileged -e DISPLAY=$DISPLAY -v '/tmp/.X11-unix':'/tmp/.X11-unix' mycontainer
```

sudo apt-get update && sudo apt-get upgrade

sudo apt-get install libusb-1.0-0-dev pkg-config

sudo apt-get install libglfw3-dev

sudo apt-get install qtcreator

sudo apt-get install qdbus qmlscene qt5-default qt5-qmake qtbase5-dev-tools qtchooser qtdeclarative5-dev xbitmaps xterm libqt5svg5-dev qttools5-dev qtscript5-dev qtdeclarative5-folderlistmodel-plugin


On the host system 

4. Reload the uvcvideo driver
  * `sudo modprobe uvcvideo`
5. Check installation by examining the last 50 lines of the dmesg log:
  * `sudo dmesg | tail -n 50`
  * The log should indicate that a new uvcvideo driver has been registered. If any errors have been noted, first attempt the patching process again, and then file an issue if not successful on the second attempt (and make sure to copy the specific error in dmesg). 
