// public domain:
// https://projects.cwi.nl/xmark/downloads.html

typedef {
    int idum,iff;
    long ix1,ix2,ix3;
    double r[98];
    int ipos;
} random_gen;

random_gen rgGlobal={.idum = -3};

int main() {
    printf("%f\n", __ranf(&rgGlobal));
    printf("%f\n", __ranf(&rgGlobal));
    printf("%f\n", ranf());
}

double __ranf(random_gen *rg) {
    if (rg->idum<0 || rg->iff==0) {
        rg->iff=1;
        rg->ix1=(54773-rg->idum) % 259200;
        rg->ix1=(7141*rg->ix1+54773) % 259200;
        rg->ix2=rg->ix1 % 134456;
        rg->ix1=(7141*rg->ix1+54773) % 259200;
        rg->ix3=rg->ix1 % 243000;
        for (int j=1;j<=97;j++) {
            rg->ix1=(7141*rg->ix1+54773) % 259200;
            rg->ix2=(8121*rg->ix2+28411) % 134456;
            rg->r[j]=(rg->ix1+rg->ix2*(1.0/134456))*(1.0/259200);
        }
        rg->idum=1;
    }

    rg->ix1=(7141*rg->ix1+54773) % 259200;
    rg->ix2=(8121*rg->ix2+28411) % 134456;
    rg->ix3=(4561*rg->ix3+51349) % 243000;
    int j=1+((97*rg->ix3)/243000);
    if (j>97 || j<1)
        printf("ranf: index out of range\n");
    double temp=rg->r[j];
    rg->r[j]=(rg->ix1+rg->ix2*(1.0/134456))*(1.0/259200);
    return temp;
}

double ranf() {
    return __ranf(&rgGlobal);
}