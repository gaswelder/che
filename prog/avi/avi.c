#import image
#import formats/avi

int main() {
	int width = 320;
	int height = 240;
	int fps = 10;

	image.image_t *img = image.new(width, height);
	image.testimage(img);

    avi.writer_t *vid = avi.start(stdout, width, height, fps);
    for (int i = 0; i < 100; i++) {
        avi.addframe(vid, img);
    }
    avi.stop(vid);

	image.free(img);
    return 0;
}
