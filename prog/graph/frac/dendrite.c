#import image
#import rnd

const int  iter = 10000;
// const double   a  = 0.00;           // slope
// const double   b  = 0.70;           // leaf size coefficient
// const double   c  = 0.70;           // dispersion
// const double   d  = 0.00;           // skew
 
pub void draw(image.image_t *img, double a, b, c, d) {
	int rad= img->width / 2;
	float x= 0.0;
	float y= 0.0;

	for (int k=1; k<=iter; k++) {
		float t= x;
		if (rnd.intn(2) == 0) {
			x=  a * x - b * y;
			y=  b * t + a * y;
		}
		else {
			x=  c * x - d * y + 1 - c;
			y=  d * t + c * y - d;
		}
		int ix = img->width / 2 + (int)(rad * x);
		int iy = img->height / 2 + (int)(rad * y);

		if (ix >= 0 && ix < img->width && iy >= 0 && iy < img->height) {
			image.set(img, ix, iy, image.white());
		}
	}
}
