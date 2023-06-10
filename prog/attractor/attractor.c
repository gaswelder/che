/*
 * Watch with  ./attractor | mpv --no-correct-pts --fps=30 -
 */
#import ppm

int main() {
    const int WIDTH = 800;
    const int HEIGHT = 600;
    const int FRAMES = 10000;
    const int iters = 10000;
    const double minX = -4.0;
    const double minY = minX * HEIGHT / WIDTH;
    const double maxX = 4.0;
    const double maxY = maxX * HEIGHT / WIDTH;
    const double minA = acos( 1.6 / 2.0 );
    const double maxA = acos( 1.3 / 2.0 );
    const double minB = acos( -0.6 / 2.0 );
    const double maxB = acos( 1.7 / 2.0 );
    const double minC = acos( -1.2 / 2.0 );
    const double maxC = acos( 0.5 / 2.0 );
    const double minD = acos( 1.6 / 2.0 );
    const double maxD = acos( 1.4 / 2.0 );


    ppm.ppm_t *ppm = ppm.init(WIDTH, HEIGHT);

    for (int i = 0; i < FRAMES; i++) {
        /*
         * Fade current picture
         */
        double sensitivity = 0.02;
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                ppm.rgb_t current_color = ppm.get(ppm, x, y);
                ppm.rgb_t out = {
                    .r = (1.0 - exp( -sensitivity * current_color.r )),
                    .g = (1.0 - exp( -sensitivity * current_color.g )),
                    .b = (1.0 - exp( -sensitivity * current_color.b ))
                };
                ppm.set(ppm, x, y, out);
            }
        }

        /*
         * Draw new picture on top
         */
        double p = (double) i / FRAMES;
        double a = cos( minA + p * (maxA - minA) ) * 2.0;
        double b = cos( minB + p * (maxB - minB) ) * 2.0;
        double c = cos( minC + p * (maxC - minC) ) * 2.0;
        double d = cos( minD + p * (maxD - minD) ) * 2.0;
        ppm.rgb_t color = create_hue(p);
        double x = 0.0;
        double y = 0.0;
        for (int j = 0; j < iters; j++) {
            /*
             * Calculate next (x, y) from current (x, y).
             *
             * xn+1 = sin(a yn) + c cos(a xn)
             * yn+1 = sin(b xn) + d cos(b yn)
             */
            double xn = sin(a * y) + c * cos(a * x);
            double yn = sin(b * x) + d * cos(b * y);
            x = xn;
            y = yn;
            
            double wtfx = (x - minX) * WIDTH / (maxX - minX);
            double wtfy = (y - minY) * HEIGHT / (maxY - minY);
            int xi = (int) wtfx;
            int yi = (int) wtfy;
            if ( xi >= 0 && xi < WIDTH && yi >= 0 && yi < HEIGHT ) {
                ppm.merge(ppm, xi, yi, color, 0.9);
            }
        }

        ppm.write(ppm, stdout);
    }

    return 0;
}

ppm.rgb_t create_hue( double h ) {
    h *= 6.0;
    double hf = h - (int) h;
    ppm.rgb_t r = {};
    switch( (int)h % 6 ) {
        case 0:
            r.r = 1.0;
            r.g = hf;
            r.b = 0.0;
            break;
        case 1:
            r.r = 1.0 - hf;
            r.g = 1.0;
            r.b = 0.0;
            break;
        case 2:
            r.r = 0.0;
            r.g = 1.0;
            r.g = hf;
            break;
        case 3:
            r.r = 0.0;
            r.g = 1.0 - hf;
            r.g = 1.0;
            break;
        case 4:
            r.r = hf;
            r.g = 0.0;
            r.g = 1.0;
            break;
        case 5:
            r.r = 1.0;
            r.g = 0.0;
            r.g = 1.0 - hf;
            break;
    }
    return r;
}
