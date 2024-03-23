#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <pthread.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define MAX_FILES 100
#define MAX_FILENAME_LENGTH 1256

#define FRAME_DELAY 100000 //microseconds

int num_substrings;
char **substrings;

char name_of_pics_downloaded[40][400];
int current_pic_index = 0;

char current_available_files[MAX_FILES][MAX_FILENAME_LENGTH];
int available_files_nr = 0;

char date[40][9];
char pic_time[40][5];

int textures_available = 0;

uint8_t downloading_in_progress = 0;
float downloading_progress = 0;
uint8_t requesting_texture_update = 0;
uint8_t texture_array_inited = 0;

#define FOLDER_OF_PICS "./pics/"

pthread_t download_thread;

// Main loop flag
uint8_t quit = 0;




void sort_strings(char strings[MAX_FILES][MAX_FILENAME_LENGTH], int num_strings) {
    // Bubble sort implementation for simplicity
    for (int i = 0; i < num_strings - 1; i++) {
        for (int j = 0; j < num_strings - i - 1; j++) {
            if (strcmp(strings[j], strings[j + 1]) > 0) {
                char temp[MAX_FILENAME_LENGTH];
                strcpy(temp, strings[j]);
                strcpy(strings[j], strings[j + 1]);
                strcpy(strings[j + 1], temp);
            }
        }
    }
}

void extract_date(const char *input_string, int nr) {
    char *token;
    char *string_copy = strdup(input_string); // Create a copy of the input string
    int iter = 0;
    // Extract the first substring
    token = strtok(string_copy, ".");
    while (token != NULL) {
        iter++;
        if(iter == 3){
            strcpy(date[nr], token);
            break;
        }
        token = strtok(NULL, "."); // Extract the next substring
    }

    free(string_copy); // Free the allocated memory
}


void extract_time(const char *input_string, int nr) {
    char *token;
    char *string_copy = strdup(input_string); // Create a copy of the input string
    int iter = 0;
    
    // Extract the first substring
    token = strtok(string_copy, ".");
    while (token != NULL) {
        iter++;
        if(iter == 4){
            strcpy(pic_time[nr], token);
            break;
        }
        token = strtok(NULL, "."); // Extract the next substring
    }

    free(string_copy); // Free the allocated memory
}

// Function to list contents of a folder and store file names
void list_files(const char *folder_path) {
    DIR *dir;
    struct dirent *entry;

    // Open the directory
    printf("Opening directory: %s\n",folder_path);
    dir = opendir(folder_path);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s\n", folder_path);
        exit(EXIT_FAILURE);
    }

    // Initialize the number of files found
    available_files_nr = 0;

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Exclude "." and ".." directories
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, ".." )) {
            // Store file name in both array of strings and array of characters
            strncpy(&current_available_files[available_files_nr][0], entry->d_name, MAX_FILENAME_LENGTH);
            printf("file no:%d  ", available_files_nr);
            printf("file name:%s\n", &current_available_files[available_files_nr][0]);

            available_files_nr++;

            // Check if the maximum number of files has been reached
            if (available_files_nr >= MAX_FILES) {
                fprintf(stderr, "Maximum number of files reached.\n");
                break;
            }
        }
    }

    // Close the directory
    closedir(dir);
}

// Function to delete all PNG files in a folder
void delete_png_files(const char *folder_path) {
    DIR *dir;
    struct dirent *entry;

    // Open the directory
    dir = opendir(folder_path);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s\n", folder_path);
        exit(EXIT_FAILURE);
    }

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Check if the file name ends with ".png"
        if (strstr(entry->d_name, ".png") != NULL) {
            // Construct the full path to the file
            char file_path[1256]; // Adjust the buffer size as needed
            snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, entry->d_name);

            // Delete the file
            if (unlink(file_path) == 0) {
                printf("Deleted file: %s\n", file_path);
            } else {
                fprintf(stderr, "Error deleting file: %s\n", file_path);
            }
        }
    }

    // Close the directory
    closedir(dir);
}

// Function to free memory allocated for file names
void free_file_names(char *file_names[MAX_FILES], int num_files) {
    for (int i = 0; i < num_files; i++) {
        free(file_names[i]);
    }
}


// Callback function to write received data into a file
size_t write_data_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

// Function to download content from a URL and store it in a file
int download_to_file(const char *url, const char *filename) {
    CURL *curl;
    CURLcode res;
    char final_destination[410];
    strcpy(final_destination, FOLDER_OF_PICS);
    strcat(final_destination, filename);
    
    
    FILE *file = fopen(final_destination, "wb"); // Open file in binary write mode
    if (!file) {
        fprintf(stderr, "Failed to open file for writing.\n");
        return 1; // Return an error code
    }

    // Initialize libcurl
    curl = curl_easy_init();
    if (curl) {
        // Set the URL for the request
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set the write callback function to write received data into the file
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback);

        // Set the file handle as the data to write to
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

        // Perform the request
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            fclose(file); // Close the file before returning
            return 1; // Return an error code
        }

        // Clean up libcurl
        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Failed to initialize libcurl.\n");
        fclose(file); // Close the file before returning
        return 1; // Return an error code
    }

    // Close the file after writing
    fclose(file);
    return 0; // Return success
}


// Function to find substrings starting with "http" and ending with ".png"
char** find_http_png_substrings(const char *input_string, int *num_substrings) {

    // Allocate memory for an array of pointers to store substrings
    char **substrings = malloc(sizeof(char*) * strlen(input_string));
    if (substrings == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Initialize the number of substrings found
    *num_substrings = 0;

    // Find substrings starting with "http" and ending with ".png"
    const char *start_ptr = input_string;
    while ((start_ptr = strstr(start_ptr, "http")) != NULL) {
        const char *end_ptr = strstr(start_ptr, ".png");
        if (end_ptr != NULL) {
            // Calculate the length of the substring
            int substring_length = end_ptr - start_ptr + 4; // Include the length of ".png"

            // Allocate memory for the substring
            char *substring = malloc(sizeof(char) * (substring_length + 1)); // +1 for the null terminator
            if (substring == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                exit(EXIT_FAILURE);
            }

            // Copy the substring
            strncpy(substring, start_ptr, substring_length);
            substring[substring_length] = '\0'; // Null-terminate the substring

            // Store the substring in the array
            substrings[*num_substrings] = substring;
            (*num_substrings)++;

            // Move to the next position in the input string
            start_ptr = end_ptr + 4; // Move past ".png"
        } else {
            break; // No more occurrences of ".png" in the input string
        }
    }

    return substrings;
}

// Function to free memory allocated for substrings
void free_substrings(char **substrings, int num_substrings) {
    for (int i = 0; i < num_substrings; i++) {
        free(substrings[i]);
    }
    free(substrings);
}

// Function to find substring starting with "mos.live" and ending with ".png"
int find_mos_live_png_substrings(const char *input_string, char *out_str) {

    // Find substrings starting with "http" and ending with ".png"
    const char *start_ptr = input_string;
    while ((start_ptr = strstr(start_ptr, "mos.live")) != NULL) {
        const char *end_ptr = strstr(start_ptr, ".png");
        if (end_ptr != NULL) {
            // Calculate the length of the substring
            int substring_length = end_ptr - start_ptr + 4; // Include the length of ".png"

            // Allocate memory for the substring
            char *substring = malloc(sizeof(char) * (substring_length + 1)); // +1 for the null terminator
            if (substring == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                exit(EXIT_FAILURE);
            }

            // Copy the substring
            if(out_str){
                strncpy(out_str, start_ptr, substring_length);
                out_str[substring_length] = '\0'; // Null-terminate the substring
            }
            
            return 0;
            
        } else {
            break; // No more occurrences of ".png" in the input string
        }
    }

    return -1;
}

// Callback function to write received data into a string
size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data) {
    size_t total_size = size * nmemb;
    strncpy(data + strlen(data), ptr, total_size);
    return total_size;
}

// Function to remove all occurrences of a character from a string
void remove_character(char *str, char c) {
    if (str == NULL)
        return;

    char *src = str;
    char *dst = str;

    // Traverse the string and copy characters excluding the specified character
    while (*src != '\0') {
        if (*src != c) {
            *dst = *src;
            dst++;
        }
        src++;
    }
    *dst = '\0'; // Null-terminate the modified string
}

void export_progress(int current, int total){
    downloading_progress = (float)current / (float)total;
    printf("dbg progress %f %d  %d \n", downloading_progress, current, total );
}

// Function to download JSON from a URL and extract links
void download_and_extract_links(const char *url) {
    CURL *curl;
    CURLcode res;
    char error_buffer[CURL_ERROR_SIZE];
    char buffer[4096*100]; // Buffer for writing received data
    int i;


    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // Set the URL for the request
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set the write callback function to capture the response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

        // Provide a buffer to store the response data
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

        // Set error buffer to store any error during request
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            fprintf(stderr, "Error: %s\n", error_buffer);
        } else {
            substrings = find_http_png_substrings(buffer, &num_substrings);
            // Print the found substrings
            printf("Found %d substrings:\n", num_substrings);
            for (i = 0; i < num_substrings; i++) {
                export_progress(i, num_substrings);
                remove_character(substrings[i],'\\');
                printf("%s\n", substrings[i]);
                if(find_mos_live_png_substrings(substrings[i], name_of_pics_downloaded[i] )){
                    fprintf(stderr, "Failed to extract filename from link.\n");
                }else{
                    //download the pic
                    if(download_to_file(substrings[i], name_of_pics_downloaded[i])){
                        fprintf(stderr, "Failed to download %s.\n", name_of_pics_downloaded[i]);
                    }else{
                        printf("Downloaded %s\n", name_of_pics_downloaded[i]);                
                    }
                }
            }

            // Free memory allocated for substrings
            free_substrings(substrings, num_substrings);
        }
        // Clean up libcurl
        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Failed to initialize libcurl.\n");
    }

    // Clean up libcurl
    curl_global_cleanup();
}



// Function to load a PNG image with transparency
SDL_Texture* load_texture_with_transparency(SDL_Renderer *renderer, const char *file_path) {
    SDL_Surface *surface = IMG_Load(file_path);
    if (!surface) {
        printf("Error loading image: %s\n", IMG_GetError());
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface); // Free the surface since the texture has been created

    if (!texture) {
        printf("Error creating texture: %s\n", SDL_GetError());
        return NULL;
    }

    return texture;
}

uint8_t add_hours_offset(char* time_str, int offset_hours) {
    // Extract hours and minutes from the input string
    int hours = atoi(strndup(time_str, 2));
    int minutes = atoi(strndup(time_str + 2, 2));

    // Add hours offset
    hours = (hours + offset_hours);
    
    // Check if rollover occurred
    uint8_t rollover = 0;
    if(hours > 23){
        rollover = 1;
        printf("Rollover occured! %d %d\n", hours - offset_hours, minutes);
        hours = hours % 24;
    }

    // Convert hours and minutes back to string
    sprintf(time_str, "%02d%02d", hours, minutes);

    return rollover;
}

void add_days_offset(char* date_str, int offset_days) {
    // Extract year, month, and day from the input string
    int year = atoi(strndup(date_str, 4));
    int month = atoi(strndup(date_str + 4, 2));
    int day = atoi(strndup(date_str + 6, 2));

    // Add days offset
    day += offset_days;

    // Check for rollover in days
    if (day > 31) {
        day -= 31;
        month++;
        if (month > 12) {
            month -= 12;
            year++;
        }
    }

    // Convert year, month, and day back to string
    sprintf(date_str, "%04d%02d%02d", year, month, day);
}

void adjust_timezone(void){
    uint8_t carry;
    
    for(int i = 0; i < available_files_nr; i++){
        carry = add_hours_offset(pic_time[i], 2);
        add_days_offset(date[i], carry);
    }
}

int load_new_radar_textures(SDL_Renderer *renderer, SDL_Texture* overlay_textures[]){
    textures_available = 0;
    for (int i = 0; i < available_files_nr; ++i) {
        if(texture_array_inited == 1){
            SDL_DestroyTexture(overlay_textures[i]);
        }
    }
    for (int i = 0; i < available_files_nr; ++i) {
        char temp_full_path[1000];
        temp_full_path[0] = 0;
        strcpy(temp_full_path, "./pics/");
        strcat(temp_full_path, current_available_files[i]);
        overlay_textures[i] = load_texture_with_transparency(renderer, temp_full_path);
        if (!overlay_textures[i]) {
            return -1;
        }
    }
    textures_available = 1;
    texture_array_inited = 1;
    return 0;
}

void *get_new_data(void* arg){
    downloading_progress = 0;
    const char *url = "https://www.meteoromania.ro/wp-content/plugins/meteo/json/imagini-radar.php";
    delete_png_files(FOLDER_OF_PICS);
    download_and_extract_links(url);
    list_files(FOLDER_OF_PICS);
    sort_strings(current_available_files, available_files_nr);
    
    
    for(int i = 0;  i < available_files_nr; i++){
        printf("File: %s  ", current_available_files[i]);
        extract_date(current_available_files[i], i);
        printf("Date: %s ", date[i]);
        extract_time(current_available_files[i], i);
        printf("Time: %s\n", pic_time[i]);
    } 
    printf("Pics nr: %d\n", available_files_nr);
    adjust_timezone();    
    
    downloading_in_progress = 0;
    requesting_texture_update = 1;
    pthread_exit(NULL);
}

// Function to render a progress bar
void render_progress_bar(SDL_Renderer *renderer, int x, int y, int width, int height, float progress) {
    // Draw background
    SDL_Rect backgroundRect = {x, y, width, height};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &backgroundRect);

    // Calculate progress bar width based on progress
    int progressBarWidth = (int)(width * progress);

    // Draw progress bar
    SDL_Rect progressBarRect = {x, y, progressBarWidth, height};
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &progressBarRect);
}

void render_frame_progress_bar(SDL_Renderer *renderer, int x, int y, int width, int height, float progress) {
    // Draw background
    SDL_Rect backgroundRect = {x, y, width, height};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer, &backgroundRect);

    // Calculate progress bar width based on progress
    int progressBarWidth = (int)(width * progress);

    // Draw progress bar
    SDL_Rect progressBarRect = {x, y, progressBarWidth, height};
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &progressBarRect);
}


int main() {
    const uint64_t update_interval_in_ms = 1000 * 600;
    int64_t remaining_time_till_update_in_ms = update_interval_in_ms;//10 * 1000000; //10 seconds
    
    
    SDL_Renderer * renderer;
    SDL_Texture* overlay_textures[40];
        
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    
    // Initialize SDL_image library
    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Create window
    SDL_Window* window = SDL_CreateWindow("meteoromania", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
        return 1;
    }


    // Create renderer for window
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    
    textures_available = 0;
    downloading_in_progress = 1;
    pthread_create(&download_thread, NULL, get_new_data, NULL);
    //pthread_detach(pthread_t thread);
    
    // Load the background PNG image
    SDL_Texture* background_texture = load_texture_with_transparency(renderer, "map.png");
    if (!background_texture) {
        return 1;
    }
    
    // Load the legend PNG image
    SDL_Texture* legend_texture = load_texture_with_transparency(renderer, "legend.png");
    if (!legend_texture) {
        return 1;
    }
    
    TTF_Init();
    
    // Load font (adjust font file path as needed)
    TTF_Font* font = TTF_OpenFont("hacker.otf", 10);
    
    if (!font) {
        fprintf(stderr, "Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -2;
    }

    SDL_Event e;

    int current_image_to_show = 0;
    
    const int radar_scale_h = 200, radar_scale_w = 300;
    const int radar_offset_h = -180, radar_offset_w = -60;
    SDL_Rect overlay_rect = {radar_offset_h, radar_offset_w, WINDOW_WIDTH + radar_scale_w, WINDOW_HEIGHT + radar_scale_h};
    
    SDL_Rect map_rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    
    SDL_Rect legend_rect = { WINDOW_WIDTH-362, WINDOW_HEIGHT-40, 362, 40};
    
    SDL_Rect date_rect = { 5, 15, 90, 20};
    SDL_Rect time_rect = { 110, 15, 40, 20};
    SDL_Rect remaining_time_rect = { 5, WINDOW_HEIGHT - 45, 335, 20};
    
    const int load_message_x = 30, load_message_y = 300;
    SDL_Rect load_new_radar_rect = { WINDOW_WIDTH/2-load_message_y, WINDOW_HEIGHT/2-load_message_x, 2*load_message_y, 2*load_message_x};
    // Create surface from string
    SDL_Color textColor_message = {200, 200, 200}; 
    SDL_Surface* messageSurface = TTF_RenderText_Solid(font, "Downloading new radar data...", textColor_message);
    if (!messageSurface) {
        fprintf(stderr, "Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return -3;
    }
    
    // Create texture from surface
    SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(renderer, messageSurface);
    if (!messageTexture) {
        fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
        return -4;
    }
    free(messageSurface);
    
    // Main loop
    while (!quit) {
        
        if(requesting_texture_update){
            // Load list of PNG images to overlay with transparency
            if(load_new_radar_textures(renderer, overlay_textures)){
                printf("Failed to load radar textures!\n");
                quit = 1;
            }
            printf("Loaded all images\n");
            textures_available = 1;
            requesting_texture_update = 0;
        }
        
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }
        
        SDL_Surface* dateSurface;
        SDL_Texture* dateTexture;
        SDL_Surface* timeSurface;
        SDL_Texture* timeTexture;
        SDL_Surface* remainingTimeSurface;
        SDL_Texture* remainingTimeTexture;
        if(textures_available){
        // Create surface from string
        SDL_Color textColor = {0, 255, 0}; 
        dateSurface = TTF_RenderText_Solid(font, date[current_image_to_show], textColor);
        if (!dateSurface) {
            fprintf(stderr, "Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
            return -3;
        }
        
        // Create texture from surface
        dateTexture = SDL_CreateTextureFromSurface(renderer, dateSurface);
        if (!dateTexture) {
            fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
            return -4;
        }
        free(dateSurface);
        
        // Create surface from string
        //SDL_Color textColor = {0, 255, 0}; 
        timeSurface = TTF_RenderText_Solid(font, pic_time[current_image_to_show], textColor);
        if (!timeSurface) {
            fprintf(stderr, "Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
            return -3;
        }
        
        // Create texture from surface
        timeTexture = SDL_CreateTextureFromSurface(renderer, timeSurface);
        if (!timeTexture) {
            fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
            return -4;
        }
        free(timeSurface);
        }
        
        // Create string for remaining time
        char rem_time_str[100];
        sprintf(rem_time_str, "Remaining time for data update: %d,%d s",remaining_time_till_update_in_ms/1000, (remaining_time_till_update_in_ms%1000)/100 );
        // Create surface from string
        SDL_Color remTextColor = {200, 255, 200}; 
        remainingTimeSurface = TTF_RenderText_Solid(font, rem_time_str, remTextColor);
        if (!remainingTimeSurface) {
            fprintf(stderr, "Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
            return -3;
        }
        
        // Create texture from surface
        remainingTimeTexture = SDL_CreateTextureFromSurface(renderer, remainingTimeSurface);
        if (!remainingTimeTexture) {
            fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
            return -4;
        }
        free(remainingTimeSurface);

        // Clear screen
        SDL_RenderClear(renderer);

        // Render background texture
        SDL_RenderCopy(renderer, background_texture, NULL, &map_rect);
        
        if(textures_available){
            // Render the overlay image
            SDL_RenderCopy(renderer, overlay_textures[current_image_to_show], NULL, &overlay_rect);
        }
        
        // Render legend texture
        SDL_RenderCopy(renderer, legend_texture, NULL, &legend_rect);
        
        if(textures_available){
        //Render text texture
        SDL_RenderCopy(renderer, dateTexture, NULL, &date_rect);
        
        //Render text texture
        SDL_RenderCopy(renderer, timeTexture, NULL, &time_rect);
        }
        
        //Render text texture
        SDL_RenderCopy(renderer, remainingTimeTexture, NULL, &remaining_time_rect);
        
        if(downloading_in_progress == 1){
            //Render message texture
            SDL_RenderCopy(renderer, messageTexture, NULL, &load_new_radar_rect);
            render_progress_bar(renderer,  WINDOW_WIDTH/2 - 400/2, WINDOW_HEIGHT/2 + 20, 400, 20, downloading_progress);
        }
        
        if(textures_available){
        //show current render frame progress
        render_frame_progress_bar(renderer,  0, WINDOW_HEIGHT - 20, 100, 20, (float) ((float)current_image_to_show/(float)available_files_nr));
        }

        // Update screen
        SDL_RenderPresent(renderer);
        
        if(textures_available){
            SDL_DestroyTexture(dateTexture);
            SDL_DestroyTexture(timeTexture);
        }
        
        SDL_DestroyTexture(remainingTimeTexture);
        
        if(remaining_time_till_update_in_ms <= 0 &&(downloading_in_progress == 0)){ //time elapsed, ready for new download
            downloading_in_progress = 1;
            pthread_create(&download_thread, NULL, get_new_data, NULL);
            //pthread_detach(pthread_t thread);
            remaining_time_till_update_in_ms = update_interval_in_ms;
        }
        
        if(textures_available == 1){
            if(current_image_to_show == available_files_nr - 1){
                current_image_to_show = 0;
            }else{
                current_image_to_show++;
            }
        }
        
        if(remaining_time_till_update_in_ms > 0){
            remaining_time_till_update_in_ms = remaining_time_till_update_in_ms - FRAME_DELAY/1000;
        }else{
            remaining_time_till_update_in_ms = 0;
        }
        usleep(FRAME_DELAY);
    }

    // Free loaded textures
    SDL_DestroyTexture(background_texture);
    SDL_DestroyTexture(legend_texture);
    for (int i = 0; i < available_files_nr; ++i) {
        SDL_DestroyTexture(overlay_textures[i]);
    }

    // Destroy renderer
    SDL_DestroyRenderer(renderer);

    // Destroy window
    SDL_DestroyWindow(window);

    // Quit SDL subsystems
    SDL_Quit();

    pthread_join(download_thread, NULL);
   
    return 0;
}
