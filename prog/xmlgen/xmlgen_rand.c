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
                res = rnd.uniform(pd->min, pd->max);
                break;
            case 3:
                res = pd->min + rnd.exponential(pd->mean);
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

pub int __ignuin(random_gen *rg,int low, int high) {
    int f=(int)(__ranf(rg)*(high-low+1));
    return low+f;
}

pub int ignuin(int low, int high) {
    random_gen *rg = &rgGlobal;
    int f=(int)(__ranf(rg)*(high-low+1));
    return low+f;
}
