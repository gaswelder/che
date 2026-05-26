#import image
 
pub void draw(image.image_t *img, int it) { 
	double x=0;
	double y=0;
	int n=0;
	int hw = img->width / 2;
	int hh = img->height / 2;
	for (int i = 0; i < it; i++) {
		double t=x;
		x=y-sin(x);
		y=3.14-t;
		n++;
		image.set(img, hw + (int)(x*2), hh + (int)(y*2), image.white());
	}
}
