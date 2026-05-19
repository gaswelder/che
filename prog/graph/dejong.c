#import att/dejong.c
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
		dejong.draw(img, a, b, c, d);
		render.push(img);
		a += rnd.u() / 1000;
		b += rnd.u() / 1000;
		c += rnd.u() / 1000;
		d += rnd.u() / 1000;
		image.apply(img, fade);
	}
	image.free(img);
	return 0;
}

void fade(image.rgba_t *c) {
	c->red /= 2;
	c->green /= 2;
	c->blue /= 2;
}
