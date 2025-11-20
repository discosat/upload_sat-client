# DTP Client-side Uploading Node 
This software is to be used as a client-side (satellite) node for the <b>DISCO-2</b> CubeSat. The purpose of this node is to enable requesting a download from the Ground Station to the satellite DISCO2; in practice, we will have a reverse server-client relationship between the Ground Station and DISCO2.

For the server-side node to be used by the ground station, please check out the [Upload Ground-Station Server](https://github.com/discosat/upload_gs-server).

# How to run
To start the Client on the DISCO2, run this command:

``` ./usr/bin/upload_sat-client -c can0 -a <CLIENT_ADDRESS> -s <SERVER_ADDRESS> ```

<b>CLIENT_ADDRESS</b> is the address of this Client, while <b>SERVER_ADDRESS</b> is the address of the Groundstation Server. 
