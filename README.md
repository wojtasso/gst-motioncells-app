# Gstreamer motioncells application

This application shows how to use gstreamer motioncells plugin. It performs motion detection on videos.

### Prerequisites

```
sudo apt-get install build-essential gstreamer1.0 libgstreamer1.0-dev libcairo2-dev
```

### Installing

Download and compile the code:
```
git clone https://github.com/wojtasso/gst-motioncells-app.git
cd gst-motioncells-application
```

The application can process frames grabbed from /dev/video0 or from udp port using gstreamer elements.

To read video from camera, connect USB camera to the computer and execute:
```
make
./motion-detect
```
And you should get video from your camera with working motioncells plugin.

To read video from udp packets from the network:
```
make udpsrc
./motion-detect
```

Then on the remote computer (e.g. Raspberyy PI) connect USB camera and invoke (Remember to provide ip address of your host computer):
```
gst-launch-1.0 v4l2src ! 'video/x-raw, width=640, height=480, framerate=30/1' ! videoconvert ! x264enc pass=qual quantizer=20 tune=zerolatency ! rtph264pay ! udpsink host=HOST_IP_ADDR port=5000
```

There can be a small delay, but you should get on the screen view of the video with working motioncells plugin.
This application is able to parse messages from xvimagesink. Using that you can mark only a specific region on the screen where motion will be detected
