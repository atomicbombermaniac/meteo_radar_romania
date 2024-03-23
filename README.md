# meteo_radar_romania

This is a quick and "simple" program that uses the weather radar available at https://www.meteoromania.ro/radarm/radar.index.php .
I wanted to have the information available without a web browser.

It uses SDL2 and curl.

gcc  -Wall  -o meteo meteo.c -pthread -lcurl -lSDL2 -lSDL2_image -lSDL2_ttf

At start it deletes all contents of the "./pics" folder, then queries and downloads the available weather radar image from the server back into that folder and start overlaying them (correctly-ish scaled) in a loop over the map. After 10 minutes the process repeats.

Each picture has the date and time embedded in the name, so that information is also overlayed on the map (top left). Same for the loop progression (lower left corner), where you can also find the countdown 'till the next update from the server.
