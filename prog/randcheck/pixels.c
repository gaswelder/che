#import formats/png
#import image
#import rnd

// Here we treat the generated numbers as pixel coordinates on an image.
// Every time a pixel is selected, we make it darker.

pub int main() {
	image.image_t *img = image.new(1040, 1040);
	image.fill(img, image.white());
	image.rgba_t color = {};
	float alpha = 1.0/64;
	for (uint32_t i = 0; i < 100000000; i++) {
		uint32_t s = rnd.intn(0xFFFFFFFF);
		uint32_t x = (s % 0x0000FFFF) / 64;
		uint32_t y = (s / 0x0000FFFF) / 64;
		image.blend(img, x, y, color, alpha);
	}
	png.write(img, "out.png", png.PNG_GRAYSCALE);
	return 0;
}
