# Jetson with 0.96 OLED Display (SSD1306)
This time not a Python file, but a cpp program.
But the problem remains, SMBUS calls don't work.
Writing data is just successful byte wise, no big data chunks possible.

Tegrastat can't run in parallel.

CPU and GPU Performance gets displayed with 9 bar graphs.
1 GPU, 8 x CPU.

## Compile file:
<pre>
$ g++ ssd1306_jetson.cpp -o oled_display
</pre>

## Run file:
<pre>
$ sudo ./oled_display
</pre>

## Check I2C Bus:

<pre>
$ sudo i2cdetect -y -r 8
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- -- 
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
30: -- -- -- -- -- -- -- -- -- -- -- -- 3c -- -- -- 
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
</pre>
