#import panic
#import rnd

// public domain:
// https://projects.cwi.nl/xmark/downloads.html

pub typedef {
    int idum,iff;
    long ix1,ix2,ix3;
    double r[98];
    int ipos;
} random_gen;

pub typedef {
    int type;
    double mean, dev, min, max;
} ProbDesc;

random_gen rgGlobal={.idum = -3};

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

pub double GenRandomNum(ProbDesc *pd)
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
                exit(1);
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
                res = pd->mean + pd->dev * rnd.gauss();
                if (res > pd->max) res = pd->max;
                if (res < pd->min) res = pd->min;
                break;
            default:
                fprintf(stderr,"woops! undefined distribution.\n");
                exit(1);
        }
    return res;
}

pub float genexp(float av)
{
    return __genexp(&rgGlobal,av);
}

float __genexp(random_gen *rg, float av) {
    return __sexpo(rg) * av;
}

pub float genunf(float low, float high) {
    return __genunf(&rgGlobal,low,high);
}

float __genunf(random_gen *rg,float low,float high) {
    if (low > high) {
        panic.panic("LOW > HIGH in GENUNF: LOW %16.6E HIGH: %16.6E\n",low,high);
    }
    return low + (high-low) * __ranf(rg);
}

pub int __ignuin(random_gen *rg,int low, int high) {
    int f=(int)(__ranf(rg)*(high-low+1));
    return low+f;
}

pub int ignuin(int low, int high) {
    return __ignuin(&rgGlobal,low,high);
}