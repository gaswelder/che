
const int NX = 10000;
const int NY = 10000;
const double M_PI = 3.141592653589793;

int main() {
    int16_t *values = calloc(NX*NY, sizeof(int16_t));

    srand(time(NULL));
    double cr = (rand() % 10000) / 500.0 - 10; // -10 -> 10;
    double ci = (rand() % 10000) / 500.0 - 10;
    double xmin = -M_PI;
    double xmax =  M_PI;
    double ymin = -M_PI;
    double ymax =  M_PI;

    for (int i = 0; i < NX; i++) {
        // map i=[0..NX] to zr=[xmin..xmax]
        double zr = xmin + i * (xmax - xmin) / NX;
        for (int j = 0; j < NY; j++) {
            // map j=[0..NY] to zi=[ymin..ymax]
            double zi = ymin + j * (ymax - ymin) / NY;
            values[j*NX+i] = get_sample(ci, cr, zi, zr);
        }
    }

    FILE *fptr = fopen("thorn.raw", "w");
    if (fptr == NULL) {
        fprintf(stderr, "Unable to create thorn.raw\n");
        return 1;
    }
    for (int j=0; j<NY; j++) {
        for (int i=0;i<NX;i++) {
            fputc(values[j*NX+i],fptr);
        }
    }
    fclose(fptr);
	return 0;
}

int get_sample(double ci, cr, zi, zr) {
    double ir = zr;
    double ii = zi;
    int k = 0;
    for (k = 0; k < 255; k++) {
        double a = ir;
        double b = ii;
        ir = a / cos(b) + cr;
        ii = b / sin(a) + ci;
        if (ir*ir + ii*ii > 10000) {
            break;
        }
    }
    return k;
}
