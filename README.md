# meteo_radar_romania

This is a quick and "simple" program that uses the weather radar available at https://www.meteoromania.ro/radarm/radar.index.php .
I wanted to have the information available without a web browser.

gcc  -Wall  -o meteo meteo.c -pthread -lcurl -lSDL2 -lSDL2_image -lSDL2_ttf
