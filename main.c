#include <stdlib.h>
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
    printf("Extracting frames...\n");
    system("mkdir -p input output");
    system("rm -f input/* output/*");
    system("ffmpeg -i demos/loco_departure.mov -vf scale=\"iw/4:ih/4\" input/%04d.png -v quiet -stats"); // Downsample 4x for performance

    struct Frame base_frame = get_frame(1);

    struct Frame output_frame = base_frame;
    output_frame.n = 3;
    int frame_size = base_frame.w * base_frame.h; 
    output_frame.data = calloc(frame_size, output_frame.n);

    int frame_number = 1;
    while (frame_exists(frame_number+1)) {
        struct Frame next_frame = get_frame(frame_number+1);

        for (int i = 0; i < frame_size; i++) {
            int pixel_old = base_frame.data[i]; // Cast to signed int for comparison
            int pixel_new = next_frame.data[i];
            
            // If luminance changed then set pixel
            // if ((pixel_new - pixel_old) > threshold) output_frame.data[i] = 255; // Brighter
            // else if ((pixel_old - pixel_new) > threshold) output_frame.data[i] = 0; // Darker
            // else output_frame.data[i] = 127;

            output_frame.data[(i*3)+0] = 0;
            output_frame.data[(i*3)+1] = 0;
            output_frame.data[(i*3)+2] = 0;
            if ((pixel_new - pixel_old) > threshold) output_frame.data[(i*3)+1] = 255; // Brighter
            else if ((pixel_old - pixel_new) > threshold) output_frame.data[(i*3)+0] = 255; // Darker
            else {
                output_frame.data[(i*3)+0] = pixel_old/2;
                output_frame.data[(i*3)+1] = pixel_old/2;
                output_frame.data[(i*3)+2] = pixel_old/2;
            }
        }

        save_frame(output_frame, frame_number);

        free_frame(base_frame);
        base_frame = next_frame;

        frame_number += 1;
    }

    // Output last frame again so input and output have same frame total
    save_frame(output_frame, frame_number);

    free_frame(base_frame);
    free_frame(output_frame);

    printf("Generating video...\n");
    system("rm output_*.mp4");
    system("ffmpeg -i output/%04d.png -c:v libx264 output_1.mp4 -v quiet -stats");
    system("ffmpeg -i input/%04d.png -i output/%04d.png -filter_complex hstack -c:v libx264 output_2.mp4 -v quiet -stats");
    // system("ffmpeg -i input/%04d.png -i output/%04d.png -filter_complex vstack -c:v libx264 output_2.mp4 -v quiet -stats");
    // system("rm -r input output");

    // ffmpeg -i demos/loco_bottle.mov -i output/%04d.png -filter_complex hstack output_3.mp4 -vsync 2

    return 0;
}