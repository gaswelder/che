#import strings
#import opt
#import words.c
#import rnd
#import ipsum.c
#import schema.c

FILE *xmlout=0;
char *global_outputname=0;
int indent_inc=0;
int stackdepth=0;
int global_split=0;
int splitcnt=0;

int GenContents_lstname = 0;
int GenContents_country = -1;
int GenContents_email = 0;
int GenContents_quantity = 0;
double GenContents_initial = 0;
double GenContents_increases = 0;
char *GenContents_auction_type[]={"Regular","Featured"};
char *GenContents_education[]={"High School","College", "Graduate School","Other"};
char *GenContents_money[]={"Money order","Creditcard", "Personal Check","Cash"};
char *GenContents_yesno[]={"Yes","No"};

typedef int printfunc_t(FILE *, const char *, ...);

printfunc_t *xmlprintf = &fprintf;

int global_split_fileno = 0;

const int COUNTRIES_USA = 0;

schema.ObjDesc *stack[64] = {};

int main(int argc, char **argv)
{
    bool dumpdtd = false;
    bool doctype_is_2 = false;
    bool show_version = false;
    bool iflag = false;
    float global_scale_factor = 1;

    opt.opt_bool("e", "dumpdtd", &dumpdtd);
    opt.opt_bool("d", "document_type=2", &doctype_is_2);    
    opt.opt_bool("v", "show version", &show_version);
    opt.opt_bool("i", "indent_inc=2", &iflag);
    opt.opt_float("f", "global_scale_factor", &global_scale_factor);
    opt.opt_str("o", "global_outputname", &global_outputname);
    opt.opt_int("s", "global_split", &global_split);

    if (argc == 1) {
        opt.opt_usage();
        return 1;
    }

    opt.opt_parse(argc, argv);

    if (show_version) {
        fprintf(stderr, "This is xmlgen, version ? by Florian Waas (flw@mx4.org)");
        return 0;
    }
    if (dumpdtd) {
        char **line = schema.getdtd();
        while (*line) {
            fprintf(stdout, "%s\n", *line);
            line++;
        }
        return 0;
    }

    int document_type=1;
    if (doctype_is_2) {
        document_type=2;
    }
    if (iflag) {
        indent_inc=2;
    }
    
    xmlout=stdout;
    if (global_outputname) {
        if (global_split) {
            xmlout = OpenOutput_split(global_outputname, global_split_fileno++);
        } else {
            xmlout = fopen(global_outputname, "w");
        }
        if (!xmlout) {
            fprintf(stderr, "Can't open file %s\n", global_outputname);
            exit(1);
        }
    }

    schema.InitializeSchema(global_scale_factor);

    
    Preamble(document_type);

    GenSubtree(xmlout, schema.GetSchemaNode(1));
    fclose(xmlout);
    return 0;
}

FILE *OpenOutput_split(const char *global_outputname, int fileno) {
    if (fileno > 99999) {
        fprintf(stderr,"Warning: More than %d files.\n", 99999);
    }
    char *newname = strings.newstr("%s%0*d", global_outputname, 5, fileno);
    FILE *f = fopen(newname,"w");
    free(newname);
    return f;
}

schema.ProbDesc global_GenRef_pdnew = {};

int GenRef(schema.ProbDesc *pd, int type)
{
    schema.ObjDesc* od = schema.GetSchemaNode(type);
    if (pd->type!=0)
        {
            global_GenRef_pdnew.min=0;
            global_GenRef_pdnew.max=od->set.size-1;
            global_GenRef_pdnew.type=pd->type;
            if (pd->type!=1)
                {
                    global_GenRef_pdnew.mean=pd->mean*global_GenRef_pdnew.max;
                    global_GenRef_pdnew.dev=pd->dev*global_GenRef_pdnew.max;
                }
        }
    return (int)GenRandomNum(&global_GenRef_pdnew);
}

enum {
    ATTR_TYPE_1 = 1,
    ATTR_TYPE_2 = 2,
    ATTR_TYPE_3 = 3
};

void OpeningTag(schema.ObjDesc *od)
{
    schema.AttDesc *att=0;
    stack[stackdepth++]=od;
    xmlprintf(xmlout,"<%s",od->name);
    for (int i=0;i<5;i++) {
        
        att=&od->att[i];
        if (att->name[0]=='\0') {
            break;
        }

        char *attname;
        if (att->name[0]=='\1') {
            attname = schema.GetSchemaNode(att->ref)->name;
        } else {
            attname=att->name;
        }

        switch(att->type) {
            case ATTR_TYPE_1:
                xmlprintf(xmlout," %s=\"%s%d\"", attname,od->name,od->set.id++);
                break;
            case ATTR_TYPE_2:
                int ref=0;
                if (!ItemIdRef(od, &ref)) {
                    ref=GenRef(&att->pd,att->ref);
                }
                xmlprintf(xmlout," %s=\"%s%d\"", attname, schema.GetSchemaNode(att->ref)->name, ref);
            break;
            case ATTR_TYPE_3:
                if (rnd.uniform(0, 1) < att->prcnt) {
                    if (!strcmp(attname,"income")) {
                        double d = 40000 + 30000 * rnd.gauss();
                        if (d < 9876) {
                            d = 9876;
                        }
                        xmlprintf(xmlout," %s=\"%.2f\"",attname, d);
                    } else {
                        xmlprintf(xmlout," %s=\"yes\"",attname);
                    }
                }
                break;
            default:
                fflush(xmlout);
                fprintf(stderr,"unknown ATT type %s\n",attname);
                exit(1);
        }
    }
    if (od->elm[0].id == 0 && (od->att[0].name[0])) {
        xmlprintf(xmlout,"/>\n");
    } else {
        xmlprintf(xmlout,">");
        if (od->elm[0].id != 0 || od->type & 0x01) {
            xmlprintf(xmlout,"\n");
        }
    }
}

void ClosingTag(schema.ObjDesc *od)
{
    stackdepth--;
    if (od->type & 0x01) {
        xmlprintf(xmlout,"\n");
    }
    if ((od->att[0].name[0]) && !(od->elm[0].id!=0)) {
        return;
    }
    xmlprintf(xmlout,"</%s>\n",od->name);
}

bool GenSubtree_splitnow = false;

void GenSubtree(FILE *out, schema.ObjDesc *od)
{
    schema.ElmDesc *ed;
    if (od->type&0x10) return;
    if (GenSubtree_splitnow) {
        // split doc
        int oldstackdepth = stackdepth;
        for (int i = oldstackdepth-1; i>=0; i--) {
            ClosingTag(stack[i]);
        }

        if (xmlout!=stdout) {
            fclose(xmlout);
        }
        if (global_outputname) {
            xmlout = OpenOutput_split(global_outputname, global_split_fileno++);
            if (!xmlout) {
                fflush(stdout);
                fprintf(stderr, "Can't open file %s\n", global_outputname);
                exit(1);
            }
        }
        for (int i=0; i<oldstackdepth; i++) {
            OpeningTag(stack[i]);
        }
        splitcnt=0;

        GenSubtree_splitnow = false;
    }

    OpeningTag(od);
    
    od->flag++;

    int r;
    bool has_content = true;
    switch(od->id) {
        case schema.CITY:
            ipsum.fcity(out); break;
        case schema.STATUS:
        case schema.HAPPINESS:
            ipsum.fint(out, 1, 10); break;
        case schema.STREET:
            ipsum.fstreet(out); break;
        case schema.PHONE:
            ipsum.fphone(out); break;
        case schema.CREDITCARD:
            ipsum.fcreditcard(out); break;
        case schema.SHIPPING:
            ipsum.fshipping(out); break;
        case schema.TIME:
            ipsum.ftime(out); break;
        case schema.AGE:
            ipsum.fage(out); break;
        case schema.ZIPCODE:
            ipsum.fzipcode(out); break;
        case schema.XDATE:
        case schema.START:
        case schema.END:
            ipsum.fdate(out); break;
        case schema.GENDER:
            ipsum.fgender(out); break;
        case schema.AMOUNT:
        case schema.PRICE:
            ipsum.fprice(out); break;

        case schema.TYPE:
            xmlprintf(out, "%s", GenContents_auction_type[rnd.range(0,1)]);
            if (GenContents_quantity>1 && rnd.range(0,1)) xmlprintf(out,", Dutch");
            break;
        case schema.LOCATION:
        case schema.COUNTRY:
            if (rnd.uniform(0, 1) < 0.75) {
                GenContents_country = COUNTRIES_USA;
            } else {
                GenContents_country = rnd.range(0, words.dictlen("countries") - 1);
            }
            xmlprintf(out, "%s", words.dictentry("countries", GenContents_country));
            break;
        case schema.PROVINCE:
            if (GenContents_country == COUNTRIES_USA) {
                ipsum.fprovince(out);
            } else {
                ipsum.flastname(out);
            }
            break;
        case schema.EDUCATION:            
            xmlprintf(out, "%s", GenContents_education[rnd.range(0,3)]);
            break;
        
        case schema.HOMEPAGE:
            xmlprintf(out, "http://www.%s/~%s",
                words.dictentry("emails", GenContents_email),
                words.dictentry("lastnames", GenContents_lstname));
            break;
        
        case schema.PAYMENT:
            r=0;
            for (int i=0;i<4;i++)
                if (rnd.range(0,1)) {
                    char *x = "";
                    if (r++) {
                        x = ", ";
                    }
                    xmlprintf(out, "%s%s", x, GenContents_money[i % 4]);
                }
            break;
        
        case schema.BUSINESS:
        case schema.PRIVACY:
            xmlprintf(out,GenContents_yesno[rnd.range(0,1)]);
            break;
        
        case schema.CATNAME:
        case schema.ITEMNAME:
            ipsum.fsentence(out, rnd.range(1,4));
            break;
        case schema.NAME:
            PrintName();
            break;
        case schema.FROM:
        case schema.TO:
            PrintName();
            xmlprintf(out," ");
            break;
        case schema.EMAIL:
            GenContents_email = rnd.range(0, words.dictlen("emails") - 1);
            xmlprintf(out, "mailto:%s@%s",
                words.dictentry("lastnames", GenContents_lstname),
                words.dictentry("emails", GenContents_email));
            break;
        
        case schema.QUANTITY:
            GenContents_quantity=1+(int)rnd.exponential(0.4);
            xmlprintf(out,"%d",GenContents_quantity);
            break;
        case schema.INCREASE:
            double d=1.5 *(1+(int)rnd.exponential(10));
            xmlprintf(out,"%.2f",d);
            GenContents_increases+=d;
        break;
        case schema.CURRENT:
            xmlprintf(out,"%.2f",GenContents_initial+GenContents_increases);
            break;
        case schema.INIT_PRICE:
            GenContents_initial=rnd.exponential(100);
            GenContents_increases=0;
            xmlprintf(out,"%.2f",GenContents_initial);
            break;
        case schema.RESERVE:
            xmlprintf(out,"%.2f",GenContents_initial*(1.2+rnd.exponential(2.5)));
            break;
        case schema.TEXT:
            PrintANY();
            break;
        default:
            has_content = false;
    }
    if (has_content && od->elm[0].id != 0) {
        xmlprintf(out,"\n");
    }

    if (od->type & 0x02) {
        schema.ObjDesc *root;
        if (od->flag > 2-1) {
            int i = 0;
            while (i < od->kids-1 && od->elm[i].rec) {
                i++;
            }
            root = schema.GetSchemaNode(od->elm[i].id);
        } else {
            double sum = 0;
            double alt = rnd.uniform(0, 1);
            int i = 0;
            while (i < od->kids-1 && (sum += od->elm[i].pd.mean) < alt) {
                i++;
            }
            root = schema.GetSchemaNode(od->elm[i].id);
        }
        GenSubtree(out, root);
    }
    else {
        for (int i = 0; i < od->kids; i++) {
            int num;
            ed = &od->elm[i];
            num = (int)(GenRandomNum(&ed->pd)+0.5);
            while (num--) {
                GenSubtree(out, schema.GetSchemaNode(ed->id));
            }
        }
    }
    ClosingTag(od);
    if (global_split) {
        if (od->type & 0x20 || (od->type & 0x40 && splitcnt++>global_split)) {
            GenSubtree_splitnow = true;
        }
    }
    od->flag--;
}

void Preamble(int type)
{
    switch(type)
        {
        case 1:
            xmlprintf(xmlout,"<?xml version=\"1.0\" standalone=\"yes\"?>\n");
            break;
        case 2:
            xmlprintf(xmlout, "<?xml version=\"1.0\"?>\n");
            xmlprintf(xmlout, "<!DOCTYPE %s SYSTEM \"auction.dtd\">\n", schema.GetSchemaNode(1)->name);
            break;
        case 3:
            xmlprintf(stderr,"Not yet implemented.\n");
            exit(1);
        }
}



char *markup[3]={"emph","keyword","bold"};
char tick[3] = "";

int PrintANY_st[3] = {};

void PrintANY() {
    int sen=1+(int)rnd.exponential(20);
    int stptr=0;
    for (int i=0;i<sen;i++)
        {
            if (rnd.uniform(0, 1) < 0.1 && stptr<3-1)
            {
                while (true) {
                    PrintANY_st[stptr]=rnd.range(0,3-1);
                    if (!tick[PrintANY_st[stptr]]) {
                        break;
                    }
                }
                tick[PrintANY_st[stptr]]=1;
                xmlprintf(xmlout,"<%s> ",markup[PrintANY_st[stptr]]);
                stptr++;
            }
            else if (rnd.uniform(0, 1) < 0.8 && stptr) {
                --stptr;
                xmlprintf(xmlout,"</%s> ",markup[PrintANY_st[stptr]]);
                tick[PrintANY_st[stptr]]=0;
            }
            ipsum.fsentence(xmlout, 1+(int)rnd.exponential(4));
       }
    while(stptr)
        {
            --stptr;
            xmlprintf(xmlout,"</%s> ",markup[PrintANY_st[stptr]]);
            tick[PrintANY_st[stptr]]=0;
        }
}

int ItemIdRef(schema.ObjDesc *odSon, int *iRef) {
    if (odSon->id != schema.ITEMREF || stackdepth < 2) {
        return 0;
    }
    schema.ObjDesc *od = stack[stackdepth-2];
    if (od->id == schema.OPEN_TRANS) {
        return GenItemIdRef(schema.getidr(0), iRef);
    }
    if (od->id == schema.CLOSED_TRANS) {
        return GenItemIdRef(schema.getidr(1), iRef);
    }
    return 0;
}

int GenItemIdRef(schema.idrepro *rep, int *idref)
{
    int res=0;
    if (rep->out>=rep->max) return 0;
    rep->out++;
    if (rep->brosout>=rep->brosmax) {
        *idref=rep->cur++;
        res=2;
    }
    else
        {
            rep->dir = rnd.range(0, 1);
            while(rep->dir!=rep->mydir && rep->brosout<rep->brosmax) {
                rep->brosout++;
                rep->cur++;
                rep->dir = rnd.range(0, 1);
            }
            *idref=rep->cur++;
            res=1;
        }
    return res;
}

// enum {
//     CATNAME,
//     EMAIL,
//     PHONE,
//     STREET,
//     CITY,
//     COUNTRY,
//     ZIPCODE,
//     PROVINCE,
//     HOMEPAGE,
//     EDUCATION,
//     GENDER,
//     BUSINESS,
//     NAME,
//     AGE,
//     CREDITCARD,
//     LOCATION,
//     QUANTITY,
//     PAYMENT,
//     SHIPPING,
//     FROM, TO,
//     XDATE,
//     ITEMNAME,
//     TEXT,
//     AMOUNT,
//     CURRENT,
//     INCREASE,
//     RESERVE,
//     MAILBOX, BIDDER, PRIVACY, ITEMREF, SELLER, TYPE, TIME,
//     STATUS, PERSONREF, INIT_PRICE, START, END, BUYER, PRICE, ANNOTATION,
//     HAPPINESS, AUTHOR
// };




void PrintName() {
    int flen = words.dictlen("firstnames");
    int fst = (int)rnd.exponential(flen/3);
    if (fst >= flen-1) {
        fst = flen-1;
    }

    int llen = words.dictlen("lastnames");
    int lst = (int)rnd.exponential(llen/3);
    if (lst >= llen-1) {
        lst = llen-1;
    }
    fprintf(xmlout,"%s %s", words.dictentry("firstnames", fst), words.dictentry("lastnames", lst));

    GenContents_lstname = lst;
}

double GenRandomNum(schema.ProbDesc *pd) {
    if (pd->max <= 0) {
        return 0;
    }

    if (pd->type == 0) {
        if (pd->min == pd->max && pd->min > 0) {
            return pd->min;
            panic("undefined probdesc");
        }
    }

    if (pd->type == 1) {
        return rnd.uniform(pd->min, pd->max);
    }

    if (pd->type == 2) {
        double res = pd->mean + pd->dev * rnd.gauss();
        if (res > pd->max) res = pd->max;
        if (res < pd->min) res = pd->min;
        return res;
    }

    if (pd->type == 3) {
        double res = pd->min + rnd.exponential(pd->mean);
        if (res > pd->max) {
            res = pd->max;
        }
        return res;
    }
    panic("woops! undefined distribution.\n");
}
