#import rnd

// public domain:
// https://projects.cwi.nl/xmark/downloads.html

pub typedef {
    int type;
    double mean, dev, min, max;
} ProbDesc;

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
