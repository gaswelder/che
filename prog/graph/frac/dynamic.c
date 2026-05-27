#import image

float a = 1;
float b = 3;
float dt = 0.1;

pub void draw(image.image_t *img) {
	b += 0.01;
	for(int i=0; i<=43; i++) {
		for(int j=0; j<=37; j++) {
			float x=i;
			float y=j;
			for (int k=1; k<=100; k++) {
				y=y + sin(x+a*sin(b*x))*dt;
				x=x - sin(y+a*sin(b*y))*dt;
				int val = 200 + ((i+j)%10);
				int ix = (int)(x*15);
				int iy = (int)(y*15);
				if (ix >= 0 && ix < img->width && iy >= 0 && iy < img->height) {
					image.set(img, ix, iy, image.gray(val));
				}
			}
		}
	}
}
