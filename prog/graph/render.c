#import formats/avi
#import image

avi.writer_t *vid = NULL;

pub void push(image.image_t *img) {
	if (!vid) {
		vid = avi.start(stdout, img->width, img->height, 10);
	}
	avi.addframe(vid, img);
}

pub void end() {
	avi.stop(vid);
}