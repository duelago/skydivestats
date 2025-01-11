# SkyWin Stats

<h1>Wall mounted instrument that displays jump data</h1>

![alti](https://github.com/user-attachments/assets/93a92640-888f-4fc3-8e9a-6e24b0e250ef)



<h2>End user installation instructions</h2>

1. Plug in the Altimeter to the wall with a usb charger and a micro usb-cable<br>
2. Search your wifi networks and connect to SkyWin Stats<br>
3. A captive portal pops up. If not start a browser and go to http://192.168.4.1<br>
4. Give your wifi credentials. The altimeter will get an IP on your wifi network<br>
5. Connect to http://skywinstats.local (You must be connected to the same wifi with your device) <br>
5b. If above does not work. Download the app Fing on your phone and scan your network to figure out the new IP for the Altimeter. (It shows up as skywinstats or Generic device) <br>
5c. Browse your Altimeter webservers IP-address. In my case I got the IP http://192.168.8.111 You have another IP<br>
6. Login on website. user: skydive  passwd: jump<br>
7. Add the url to your dropzone JSON API, SkyWinOne 24.0.1 or later. (Talk to Jesper Löfberg at SkyWin if you don't know how)<br>
   (Demo looks like this: https://demo.skywin.se/api/v1/reports/jump-stats.json )<br>
8. Done! The url is stored in EEPROM and will survive a power failure. The unit will also recalibrate itself if it loses power.

<h2>How to build your own SkyWin stats altimeter</h2>

We use a 3D printed altimeter to show jump statistics from the Skywin database<p>

Demo JSON url from SkyWin: https://demo.skywin.se/api/v1/reports/jump-stats.json<br>
Demo JSON url with more random numbers between reload: http://www.hoppaiplurret.se/jump.php
<br><br>
<b>Example output:</b><br> {"name":"jump-stats","params":"[jumpYear:2025]","result":[{"Total":114,"Other":90,"AFF student":0,"Static line":0,"Manual":0,"Tandem":0,"Video/photo (tandem)":0,"Cockpit":0,"Display":24,"Competition":0}],"size":2}<p></p>

Idea is to use a stepper motor and a hall sensor and to move the motor to represent the number of jumps made the current season. The first version here is for tandems.<br><br>
<b>Steps to do:</b>
<br>
- 3D print the parts. It is easiest on a Bambulab with AMS, but possible on a normal printer with two color changes.Print the back cover in two parts with connectors so you can open up the construction after it is finished. (I do this in Prusa slicer but it was not possible to export ready made stl files with the connectors added)<p></p>

<b>Buy the following:</b><br>
- Wemos d1 mini v3 :https://www.aliexpress.com/item/4000030041652.html<br>
- Stepper motor with driver board: 28BYJ-48-5V + Driver Board ULN2003: https://www.aliexpress.com/item/1005006781616785.html<br>
- KY-024 hall sensor: https://www.aliexpress.com/item/1005007847486915.html<br>
- Magnets: https://www.aliexpress.com/item/1005004351705901.html<br>
- M 2.5 screws<br>
- Dupont cables for soldering<br>
- Superglue to glue the 3D printed part for the components to the backside of the watchface<br>
- Microusb cable<p></p>
<img width="778" alt="koppling" src="https://github.com/user-attachments/assets/d51a984a-0ed5-4f79-a765-7448604af6c3" />


<p></p>

You need to press fit 3 magnets to the altimeter arrow. One drop of super glue is also good for safety measures so you don't lose the magnets. Make sure that you place the magnets in the correct direction. The hall sensor will only work with the correct polarity. When set, play around with the calibaration setting in the code to make sure the arrow calibrates to 0 at boot. It can differ a few degrees depending on the position of the hall sensor.


![magnet](https://github.com/user-attachments/assets/76e0444e-bc5e-41e1-bdf5-dba43e394bee)
<p></p>

![konstruktion](https://github.com/user-attachments/assets/51103a65-30be-4187-b6d7-d2adb28c1851)


<img width="1203" alt="Screenshot 2025-01-09 at 13 30 09" src="https://github.com/user-attachments/assets/0e7f977a-1ea7-419f-aa28-9c3d9fb0741a" />
<br>
<img width="1087" alt="Screenshot 2025-01-09 at 13 31 11" src="https://github.com/user-attachments/assets/898c3b2c-a9eb-4df3-b269-ea075e23708b" />
<br>
<img width="1235" alt="Screenshot 2025-01-09 at 13 37 34" src="https://github.com/user-attachments/assets/078bd448-32c3-4aed-a183-d4b6bdac3622" />
<br>
<img width="1120" alt="Screenshot 2025-01-09 at 13 37 44" src="https://github.com/user-attachments/assets/b1154d37-042d-4638-ad00-f5998d7a0614" />




