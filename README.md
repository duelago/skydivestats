# SkyWin Stats

We use a 3D printed altimeter to show jump statistics from the Skywin database<p>

Demo JSON url from SkyWin: https://demo.skywin.se/api/v1/reports/jump-stats.json<br>
Demo JSON url with more random numbers between reload: http://www.hoppaiplurret.se/jump.php
<br><br>
<b>Example output:</b><br> {"name":"jump-stats","params":"[jumpYear:2025]","result":[{"Tandem":0,"NonTandem":114}],"size":2}<p></p>

Idea is to use a stepper motor and a hall sensor and to move the motor to represent the number of jumps made the current season. The first version here is for tandems.<br><br>
<b>Steps to do:</b>
<br>
- 3D print the parts. It is easiest on a Bambulab with AMS, but possible on a normal printer with two color changes.<p></p>

<b>Buy the following:</b><br>
- Wemos d1 mini v3 :https://www.aliexpress.com/item/4000030041652.html<br>
- Stepper motor with driver board: 28BYJ-48-5V + Driver Board ULN2003: https://www.aliexpress.com/item/1005006781616785.html<br>
- KY-024 hall sensor: https://www.aliexpress.com/item/1005007847486915.html<br>
- Magnet: https://www.aliexpress.com/item/1005004351705901.html<br>
- M 2.5 screws<br>
- Dupont cables for soldering<br>
- Microusb cable<p></p>
<img width="778" alt="koppling" src="https://github.com/user-attachments/assets/d51a984a-0ed5-4f79-a765-7448604af6c3" />


<p></p>

You need to press fit the magnet to the altimeter arrow. One drop of super glue is also good for safety measures so you don't lose the magnet. Make sure that you place the magnet in the correct direction. The hall sensor will only work with the correct polarity. When working, play around with the calibaration setting in the code to make sure the arrow calibrates to 0 at boot. It can differ a few degrees depending on the position of the hall sensor.


![magnet](https://github.com/user-attachments/assets/76e0444e-bc5e-41e1-bdf5-dba43e394bee)




<img width="1203" alt="Screenshot 2025-01-09 at 13 30 09" src="https://github.com/user-attachments/assets/0e7f977a-1ea7-419f-aa28-9c3d9fb0741a" />
<br>
<img width="1087" alt="Screenshot 2025-01-09 at 13 31 11" src="https://github.com/user-attachments/assets/898c3b2c-a9eb-4df3-b269-ea075e23708b" />
<br>
<img width="1235" alt="Screenshot 2025-01-09 at 13 37 34" src="https://github.com/user-attachments/assets/078bd448-32c3-4aed-a183-d4b6bdac3622" />
<br>
<img width="1120" alt="Screenshot 2025-01-09 at 13 37 44" src="https://github.com/user-attachments/assets/b1154d37-042d-4638-ad00-f5998d7a0614" />


![konstruktion](https://github.com/user-attachments/assets/51103a65-30be-4187-b6d7-d2adb28c1851)

