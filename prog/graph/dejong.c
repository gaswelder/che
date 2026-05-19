#import image
#import render.c
#import rnd
 
double a = -2.70;
double b = -0.90;
double c = -0.86;
double d = -2.20;
 
int main() {
	image.image_t *img = image.new(800, 600);
	for (int i = 0; i < 100; i++) {
		frame(img, a, b, c, d);
		render.push(img);
		a += rnd.u() / 1000;
		b += rnd.u() / 1000;
		c += rnd.u() / 1000;
		d += rnd.u() / 1000;
		// image.clear(img);
		image.apply(img, fade);
	}
	// png.write(img, "dejong.png", png.PNG_GRAYSCALE);
	image.free(img);
	return 0;
}

void frame(image.image_t *img, double a, b, c, d) {
	image.rgba_t color = image.white();
	double x = 0;
	double y = 0;
	double x1 = 0;
	double y1 = 0;
	for (int i = 0; i < 1000000; i++) {
		x1 = sin(a * y) - cos(b * x);
		y1 = sin(c * x) - cos(d * y);
		x = x1;
		y = y1;
		image.set(img, 350 + (int)(x*120), 210 + (int)(y*120), color);
	}
}

void fade(image.rgba_t *c) {
	c->red /= 2;
	c->green /= 2;
	c->blue /= 2;
}
