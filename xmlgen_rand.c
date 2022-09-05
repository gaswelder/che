#import panic

// public domain:
// https://projects.cwi.nl/xmark/downloads.html

typedef {
    int idum,iff;
    long ix1,ix2,ix3;
    double r[98];
    int ipos;
} random_gen;

typedef {
    int type;
    double mean, dev, min, max;
} ProbDesc;

random_gen rgGlobal={.idum = -3};

int main() {
    printf("%f\n", __ranf(&rgGlobal));
    printf("%f\n", __ranf(&rgGlobal));
    printf("%f\n", ranf());
    printf("------\n");
    printf("%f\n", sexpo());
    printf("%f\n", sexpo());
    printf("%f\n", sexpo());
    printf("------ snorm\n");
    printf("%f\n", snorm());
    printf("%f\n", snorm());
    printf("%f\n", snorm());
    printf("------ gennor\n");
    printf("%lf\n", gennor(0.2, 10));
    printf("%lf\n", gennor(0.2, 10));
    printf("%lf\n", gennor(0.2, 10));

    ProbDesc pdnew;
    pdnew.min=0;
    pdnew.max=10;
    pdnew.type=2;
    pdnew.mean=20;
    pdnew.dev=1;
    printf("------ GenRandomNum\n");
    printf("%f\n", GenRandomNum(&pdnew));
    printf("%f\n", GenRandomNum(&pdnew));
    printf("%f\n", GenRandomNum(&pdnew));

    printf("------ ignuin\n");
    printf("%d\n", ignuin(10, 20));
    printf("%d\n", ignuin(10, 20));
    printf("%d\n", ignuin(10, 20));
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

float global_sexpo_q[8] = {
    0.6931472,0.9333737,0.9888778,0.9984959,0.9998293,0.9999833,0.9999986,1.0
};



float global_sexpo_umin = 0;
float *global_sexpo_q1 = global_sexpo_q;

float __sexpo(random_gen *rg)
{
    long i;
    float a = 0.0;
    float u = __ranf(rg);
    float result = 0;
    int state = 30;
    float ustar;
    
    while (true) {
        switch (state) {
            case 0:
                return result;
            case 20:
                a += *global_sexpo_q1;
                state = 30;
                break;
            case 30:
                u += u;
                if (u <= 1.0) {
                    state = 20;
                    break;
                }
                u -= 1.0;
                if (u > *global_sexpo_q1) {
                    state = 60;
                    break;
                }
                result = a+u;
                state = 0;
                break;
            case 60:
                i = 1;
                ustar = __ranf(rg);
                global_sexpo_umin = ustar;
                state = 70;
                break;
            case 70:
                ustar = __ranf(rg);
                if (ustar < global_sexpo_umin) {
                    global_sexpo_umin = ustar;
                }
                i += 1;
                if (u > *(global_sexpo_q+i-1)) {
                    state = 70;
                    break;
                }
                result = a+global_sexpo_umin**global_sexpo_q1;
                state = 0;
                break;
        }
    }
}

float sexpo()
{
    return __sexpo(&rgGlobal);
}

float snorm_A[32] = {
    0.0,3.917609E-2,7.841241E-2,0.11777,0.1573107,0.1970991,0.2372021,0.2776904,
    0.3186394,0.36013,0.4022501,0.4450965,0.4887764,0.5334097,0.5791322,
    0.626099,0.6744898,0.7245144,0.7764218,0.8305109,0.8871466,0.9467818,
    1.00999,1.077516,1.150349,1.229859,1.318011,1.417797,1.534121,1.67594,
    1.862732,2.153875
};
float snorm_D[31] = {
    0.0,0.0,0.0,0.0,0.0,0.2636843,0.2425085,0.2255674,0.2116342,0.1999243,
    0.1899108,0.1812252,0.1736014,0.1668419,0.1607967,0.1553497,0.1504094,
    0.1459026,0.14177,0.1379632,0.1344418,0.1311722,0.128126,0.1252791,
    0.1226109,0.1201036,0.1177417,0.1155119,0.1134023,0.1114027,0.1095039
};
float snorm_T[31] = {
    7.673828E-4,2.30687E-3,3.860618E-3,5.438454E-3,7.0507E-3,8.708396E-3,
    1.042357E-2,1.220953E-2,1.408125E-2,1.605579E-2,1.81529E-2,2.039573E-2,
    2.281177E-2,2.543407E-2,2.830296E-2,3.146822E-2,3.499233E-2,3.895483E-2,
    4.345878E-2,4.864035E-2,5.468334E-2,6.184222E-2,7.047983E-2,8.113195E-2,
    9.462444E-2,0.1123001,0.136498,0.1716886,0.2276241,0.330498,0.5847031
};
float snorm_H[31] = {
    3.920617E-2,3.932705E-2,3.951E-2,3.975703E-2,4.007093E-2,4.045533E-2,
    4.091481E-2,4.145507E-2,4.208311E-2,4.280748E-2,4.363863E-2,4.458932E-2,
    4.567523E-2,4.691571E-2,4.833487E-2,4.996298E-2,5.183859E-2,5.401138E-2,
    5.654656E-2,5.95313E-2,6.308489E-2,6.737503E-2,7.264544E-2,7.926471E-2,
    8.781922E-2,9.930398E-2,0.11556,0.1404344,0.1836142,0.2790016,0.7010474
};

float global_snorm = 0;
float global_snorm_u = 0;
float global_snorm_s = 0;
float global_snorm_ustar = 0;
float global_snorm_aa = 0;
float global_snowm_w = 0;
float global_snorm_y = 0;
float global_snorm_tt = 0;
long global_snorm_i = 0;

float __snorm(random_gen *rg)
{
    int state = 0;
    while (true) {
        switch (state) {
            case 0:
                global_snorm_u = __ranf(rg);
                global_snorm_s = 0.0;
                if(global_snorm_u > 0.5) global_snorm_s = 1.0;
                global_snorm_u += (global_snorm_u-global_snorm_s);
                global_snorm_u = 32.0*global_snorm_u;
                global_snorm_i = (long) (global_snorm_u);
                if(global_snorm_i == 32) global_snorm_i = 31;
                if(global_snorm_i == 0) {
                    state = 100;
                    break;
                }
                global_snorm_ustar = global_snorm_u-(float)global_snorm_i;
                global_snorm_aa = *(snorm_A + global_snorm_i-1);
                break;
            case 40:
                if(global_snorm_ustar <= *(snorm_T+global_snorm_i-1)) {
                    state = 60;
                    break;
                }
                global_snowm_w = (global_snorm_ustar-*(snorm_T + global_snorm_i-1))**(snorm_H + global_snorm_i-1);
                state = 50;
                break;
            case 50:
                global_snorm_y = global_snorm_aa+global_snowm_w;
                global_snorm = global_snorm_y;
                if(global_snorm_s == 1.0) global_snorm = -global_snorm_y;
                return global_snorm;
            case 60:
                global_snorm_u = __ranf(rg);
                global_snowm_w = global_snorm_u*(*(snorm_A+global_snorm_i)-global_snorm_aa);
                global_snorm_tt = (0.5*global_snowm_w+global_snorm_aa)*global_snowm_w;
                state = 80;
                break;
            case 70:
                global_snorm_tt = global_snorm_u;
                global_snorm_ustar = __ranf(rg);
                state = 80;
                break;
            case 80:
                if (global_snorm_ustar > global_snorm_tt) {
                    state = 50;
                    break;
                }
                global_snorm_u = __ranf(rg);
                if(global_snorm_ustar >= global_snorm_u) {
                    state = 70;
                    break;
                }
                global_snorm_ustar = __ranf(rg);
                state = 40;
                break;
            case 100:
                global_snorm_i = 6;
                global_snorm_aa = *(snorm_A+31);
                state = 120;
                break;
            case 110:
                global_snorm_aa += *(snorm_D+global_snorm_i-1);
                global_snorm_i += 1;
                state = 120;
                break;
            case 120:
                global_snorm_u += global_snorm_u;
                if (global_snorm_u < 1.0) {
                    state = 110;
                    break;
                }
                global_snorm_u -= 1.0;
                state = 140;
                break;
            case 140:
                global_snowm_w = global_snorm_u**(snorm_D+global_snorm_i-1);
                global_snorm_tt = (0.5*global_snowm_w+global_snorm_aa)*global_snowm_w;
                state = 160;
                break;
            case 150:
                global_snorm_tt = global_snorm_u;
                state = 160;
                break;
            case 160:
                global_snorm_ustar = __ranf(rg);
                if (global_snorm_ustar > global_snorm_tt) {
                    state = 50;
                    break;
                }
                global_snorm_u = __ranf(rg);
                if (global_snorm_ustar >= global_snorm_u) {
                    state = 150;
                    break;
                }
                global_snorm_u = __ranf(rg);
                state = 140;
                break;
        }
    }
}

float snorm() {
    return __snorm(&rgGlobal);
}

float __gennor(random_gen *rg,float av,float sd) {
    return sd * __snorm(rg)+av;
}
float gennor(float av, float sd) {
    return __gennor(&rgGlobal,av,sd);
}

double GenRandomNum(ProbDesc *pd)
{
    double res=0;
    if (pd->max>0)
        switch(pd->type)
            {
            case 0:
                if (pd->min==pd->max && pd->min>0)
                    {
                        res=pd->min;
                        break;
                    }
                fprintf(stderr,"undefined probdesc.\n");
                exit(EXIT_FAILURE);
            case 1:
                res=genunf(pd->min,pd->max);
                break;
            case 3:
                res = pd->min + genexp(pd->mean);
                if (res > pd->max) {
                    res = pd->max;
                }
                break;
            case 2:
                res = gennor(pd->mean,pd->dev);
                if (res > pd->max) res = pd->max;
                if (res < pd->min) res = pd->min;
                break;
            default:
                fprintf(stderr,"woops! undefined distribution.\n");
                exit(EXIT_FAILURE);
        }
    return res;
}

float genexp(float av)
{
    return __genexp(&rgGlobal,av);
}

float __genexp(random_gen *rg, float av) {
    return __sexpo(rg) * av;
}

float genunf(float low, float high) {
    return __genunf(&rgGlobal,low,high);
}

float __genunf(random_gen *rg,float low,float high) {
    if (low > high) {
        panic("LOW > HIGH in GENUNF: LOW %16.6E HIGH: %16.6E\n",low,high);
    }
    return low + (high-low) * __ranf(rg);
}

int __ignuin(random_gen *rg,int low, int high) {
    int f=(int)(__ranf(rg)*(high-low+1));
    return low+f;
}

int ignuin(int low, int high) {
    return __ignuin(&rgGlobal,low,high);
}