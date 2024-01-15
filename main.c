#include <stdlib.h>
#include <time.h>
#include "error_messages.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

unsigned char threshold = 63; // Luminance difference threshold

struct Frame {
    int i, w, h, n;
    unsigned char* data;
};

int frame_exists(int i) {
    char filename[32];
    sprintf(filename, "input/%04d.png", i);

    int ok = stbi_info(filename, NULL, NULL, NULL);
    return ok;
}

struct Frame get_frame(int i) {
    char filename[32];
    sprintf(filename, "input/%04d.png", i);

    struct Frame frame;

    int ok = stbi_info(filename, &frame.w, &frame.h, &frame.n);
    if (!ok) error("%s does not exist", filename);

    printf("Loading input frame %d: %s, %dx%d, %d channels\n", i, filename, frame.w, frame.h, frame.n);

    frame.data = stbi_load(filename, &frame.w, &frame.h, &frame.n, STBI_grey);  // Load as greyscale
    frame.n = STBI_grey;
    
    return frame; 
}

void free_frame(struct Frame frame) {
    stbi_image_free(frame.data);
}

void save_frame(struct Frame frame, int i) {
    char filename[32];
    sprintf(filename, "output/%04d.png", i);

    printf("Saving output frame %d: %s, %dx%d, %d channels\n", i, filename, frame.w, frame.h, frame.n);

    stbi_write_png(filename, frame.w, frame.h, frame.n, frame.data, 0);
}

int main() {
    struct timespec start_time, end_time;

    printf("Extracting frames...\n");
    clock_gettime(CLOCK_REALTIME, &start_time);
    system("mkdir -p input output");
    system("rm -f input/* output/*");
    system("ffmpeg -i demos/loco_testing.mov -vf scale=\"iw/2:ih/2\" input/%04d.png -v quiet -stats"); // Downsample 4x for performance
    clock_gettime(CLOCK_REALTIME, &end_time);
    double extraction_time = (end_time.tv_sec - start_time.tv_sec) + ((end_time.tv_nsec - start_time.tv_nsec)/1e9);

    int frame_count = 0;
    while (frame_exists(frame_count+1)) frame_count++;
    printf("Input is %d frames\n", frame_count);

    // Load all input frames and allocate blank output frames
    printf("Loading input frames...\n");
    clock_t load_clock = clock();
    clock_gettime(CLOCK_REALTIME, &start_time);
    struct Frame* in_frames = calloc(frame_count, sizeof(struct Frame));
    struct Frame* out_frames = calloc(frame_count, sizeof(struct Frame));
    in_frames[0] = get_frame(1);
    int pixel_count = in_frames[0].w * in_frames[0].h;
    #pragma omp parallel for
    for (int i = 0; i < frame_count; i++) {
        in_frames[i] = get_frame(i+1);
        out_frames[i] = (struct Frame){in_frames[i].i, in_frames[0].w, in_frames[0].h, 3, NULL};
        out_frames[i].data = calloc(pixel_count, out_frames[i].n);
    }
    clock_gettime(CLOCK_REALTIME, &end_time);
    double load_time = (end_time.tv_sec - start_time.tv_sec) + ((end_time.tv_nsec - start_time.tv_nsec)/1e9);
    
    // Process frames
    printf("Processing frames...\n");
    FILE* event_file = fopen("events.txt", "w");
    clock_gettime(CLOCK_REALTIME, &start_time);
    #pragma omp parallel for
    for (int i = 0; i < (frame_count-1); i++) {
        printf("Processing frame %d\n", i);
        for (int j = 0; j < pixel_count; j++) {
            int pixel_old = in_frames[i].data[j]; // Cast to signed int for comparison
            int pixel_new = in_frames[i+1].data[j];

            // If luminance changed then set pixel
            if ((pixel_new - pixel_old) > threshold){
                out_frames[i].data[(j*3)+1] = 255; // Brighter

                int x = j % in_frames[i].w;
                int y = j / in_frames[i].w;
                // fprintf(event_file, "FRAME %d, %d, %d, %d\n", i, x, y, 1);
            } else if ((pixel_old - pixel_new) > threshold) {
                out_frames[i].data[(j*3)+0] = 255; // Darker

                int x = j % in_frames[0].w;
                int y = j / in_frames[0].w;
                // fprintf(event_file, "FRAME %d, %d, %d, %d\n", i, x, y, -1);
            } else {
                out_frames[i].data[(j*3)+0] = pixel_old/2;
                out_frames[i].data[(j*3)+1] = pixel_old/2;
                out_frames[i].data[(j*3)+2] = pixel_old/2;
            }
        }
    }
    clock_gettime(CLOCK_REALTIME, &end_time);
    double process_time = (end_time.tv_sec - start_time.tv_sec) + ((end_time.tv_nsec - start_time.tv_nsec)/1e9);
    fclose(event_file);

    // Free frames and save output
    printf("Saving output frames...\n");
    clock_gettime(CLOCK_REALTIME, &start_time);
    #pragma omp parallel for
    for (int i = 0; i < frame_count; i++) {
        save_frame(out_frames[i], i+1);
        free_frame(in_frames[i]);
        free_frame(out_frames[i]);
    }
    clock_gettime(CLOCK_REALTIME, &end_time);
    double save_time = (end_time.tv_sec - start_time.tv_sec) + ((end_time.tv_nsec - start_time.tv_nsec)/1e9);

    

    printf("Generating video...\n");
    clock_gettime(CLOCK_REALTIME, &start_time);
    system("rm -f output_*.mp4");
    system("ffmpeg -i output/%04d.png -c:v libx264 output_1.mp4 -v quiet -stats");
    system("ffmpeg -i input/%04d.png -i output/%04d.png -filter_complex hstack -c:v libx264 output_2.mp4 -v quiet -stats");
    system("ffmpeg -i input/%04d.png -i output/%04d.png -filter_complex vstack -c:v libx264 output_3.mp4 -v quiet -stats");
    system("rm -r input output");
    clock_gettime(CLOCK_REALTIME, &end_time);
    double generation_time = (end_time.tv_sec - start_time.tv_sec) + ((end_time.tv_nsec - start_time.tv_nsec)/1e9);

    printf("--- DONE ---\n");
    printf("Extraction took %.2f seconds\n", extraction_time);
    printf("Loading took %.2f seconds, %.1f FPS\n", load_time, frame_count/load_time);
    printf("Processing took %.2f seconds, %.1f FPS\n", process_time, frame_count/process_time);
    printf("Saving took %.2f seconds, %.1f FPS\n", save_time, frame_count/save_time);
    printf("Generation took %.2f seconds\n", generation_time);

    return 0;
}