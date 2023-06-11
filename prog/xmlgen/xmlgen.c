#import xmlgen_rand.c
#import strutil
#import opt
#import words.c
#import rnd
#import ipsum.c

typedef { int id; xmlgen_rand.ProbDesc pd; char rec; } ElmDesc;

typedef {
    char name[20];
    int type;
    int ref;
    xmlgen_rand.ProbDesc pd;
    float prcnt;
} AttDesc;

typedef { int size, id; SetDesc *next; } SetDesc;

typedef {
    int id;
    /*
     * Element name, the string inside the angle brackets.
     */
    char *name;
    ElmDesc elm[20];
    AttDesc att[5];
    int type;
    int kids;
    SetDesc set;
    int flag;
} ObjDesc;

FILE *xmlout=0;
char *global_outputname=0;
int indent_inc=0;
float global_scale_factor = 1;
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

ObjDesc *stack[64] = {};
ObjDesc objs[]={
    { .id = 0, .name = "*error*" },
    {
        .id = AUCTION_SITE,
        .name = "site",
        .elm = {
            {REGION,{1,0,0,1,1}, 0},
            {CATEGORY_LIST,{1,0,0,1,1}, 0},
            {CATGRAPH,{1,0,0,1,1}, 0},
            {PERSON_LIST,{1,0,0,1,1}, 0},
            {OPEN_TRANS_LIST,{1,0,0,1,1}, 0},
            {CLOSED_TRANS_LIST,{1,0,0,1,1}, 0}
        }
    },
    {
        .id = CATEGORY_LIST,
        .name = "categories",
        .elm = {
            {CATEGORY,{1,0,0,1000,1000}, 0}
        }
    },
    {
        .id = REGION,
        .name = "regions",
        .elm = {
            {AFRICA,{1,0,0,1,1}, 0},
            {ASIA,{1,0,0,1,1}, 0},
            {AUSTRALIA,{1,0,0,1,1}, 0},
            {EUROPE,{1,0,0,1,1}, 0},
            {NAMERICA,{1,0,0,1,1}, 0},
            {SAMERICA,{1,0,0,1,1}, 0}
        }
    },
    {
        .id = EUROPE,
        .name = "europe",
        .elm = {
            {ITEM,{1,0,0,6000,6000}, 0}
        }
    },
    {
        .id = AUSTRALIA,
        .name = "australia",
        .elm = {
            {ITEM,{1,0,0,2200,2200}, 0}
        }
    },
    {
        .id = AFRICA,
        .name = "africa",
        .elm = {
            {ITEM,{1,0,0,550,550}, 0}
        }
    },
    {
        .id = NAMERICA,
        .name = "namerica",
        .elm = {
            {ITEM,{1,0,0,10000,10000}, 0}
        }
    },
    {
        .id = SAMERICA,
        .name = "samerica",
        .elm = {
            {ITEM,{1,0,0,1000,1000}, 0}
        }
    },
    {
        .id = ASIA,
        .name = "asia",
        .elm = {
            {ITEM,{1,0,0,2000,2000}, 0}
        }
    },
    {
        .id = CATGRAPH,
        .name = "catgraph",
        .elm = {
            {EDGE,{1,0,0,3800,3800}, 0}
        },
        .att = {
            {
                .name = "",
                .type = 0,
                .ref = 0,
                .pd = {0,0,0,0,0}
            }
        },
        0x20
    },
    {
        .id = EDGE,
        .name = "edge",
        .elm = {
            {0,{0,0,0,0,0}, 0}
        },
        .att = {
            {
                .name = "from",
                .type = 2,
                CATEGORY,
                {1,0,0,0,0}
            },
            {
                .name = "to",
                .type = 2,CATEGORY,{1,0,0,0,0}}
        }
    },
    {
        .id = CATEGORY,
        .name = "category",
        .elm = {
            {CATNAME,{1,0,0,1,1}, 0},
            {DESCRIPTION,{1,0,0,1,1}, 0},
        },
        .att = {
            {
                .name = "id",
                .type = 1,0,{0,0,0,0,0}
            }
        }
    },
    {
        .id = ITEM,
        .name = "item",
        .elm = {
            {LOCATION,{1,0,0,1,1}, 0},
            {QUANTITY,{1,0,0,1,1}, 0},
            {ITEMNAME,{1,0,0,1,1}, 0},
            {PAYMENT,{1,0,0,1,1}, 0},
            {DESCRIPTION,{1,0,0,1,1}, 0},
            {SHIPPING,{1,0,0,1,1}, 0},
            {INCATEGORY,{3,3,0,1,10}, 0},
            {MAILBOX,{1,0,0,1,1}, 0}
        },
        .att = {
            {
                .name = "id",
                .type = 1,0,{0,0,0,0,0}},
            {
                .name = "featured",
                .type = 3,0,{0,0,0,0,0},0.1}
        },
        0x40
    },
    { .id = LOCATION, .name = "location" },
    { .id = QUANTITY, .name = "quantity" },
    { .id = PAYMENT, .name = "payment" },
    { .id = NAME, .name = "name" },
    { .id = ITEMNAME, .name = "name" },
    { .id = CATNAME, .name = "name" },
    {
        .id = DESCRIPTION,
        .name = "description",
        .elm = {
            {TEXT,{1,0.7,0,0,0}, 0},
            {PARLIST,{1,0.3,0,0,0}, 0}
        },
        .att = {
            {
                .name = "",
                .type = 0,
                0,{0,0,0,0,0}
            }
        },
        0x02
    },
    {
        .id = PARLIST,
        .name = "parlist",
        .elm = {
            {LISTITEM,{3,1,0,2,5}, 0}
        }
    },
    {
        .id = TEXT,
        .name = "text",
        .elm = {
            {0,{0,0,0,0,0}, 0}
        },
        .att = {{.name = "",0,0,{0,0,0,0,0}}},
        0x01
    },
    {
        .id = LISTITEM,
        .name = "listitem",
        .elm = {
            {TEXT,{1,0.8,0,0,0}, 0},
            {PARLIST,{1,0.2,0,0,0}, 0}
        },
        .att = {
            {
                .name = "",
                .type = 0,0,{0,0,0,0,0}
            }
        },
        0x02
    },
    { .id = SHIPPING, .name = "shipping" },
    { .id = RESERVE, .name = "reserve" },
    {
        .id = INCATEGORY,
        .name = "incategory",
        .elm = {{0,{0,0,0,0,0}, 0}},
        .att = {
            {
                .name = "\1",
                .type = 2,
                CATEGORY,{1,0,0,0,0}
            }
        }
    },
    {
        .id = MAILBOX,
        .name = "mailbox",
        .elm = {
            {MAIL,{3,1,0,0,250}, 0}
        }
    },
    {
        .id = MAIL,
        .name = "mail",
        .elm = {
            {FROM,{1,0,0,1,1}, 0},
            {TO,{1,0,0,1,1}, 0},
            {XDATE,{1,0,0,1,1}, 0},
            {TEXT,{1,0,0,1,1}, 0}
        }
    },
    { .id = FROM, .name = "from" },
    { .id = TO, .name = "to" },
    { .id = XDATE, .name = "date" },
    {
        .id = PERSON_LIST,
        .name = "people",
        .elm = {
            {PERSON,{1,0,0,25500,25500}, 0}
        }
    },
    {
        .id = PERSON,
        .name = "person",
        .elm = {
            {NAME,{1,0,0,1,1}, 0},
            {EMAIL,{1,0,0,1,1}, 0},
            {PHONE, {1,0,0,0,1}, 0},
            {ADDRESS, {1,0,0,0,1}, 0},
            {HOMEPAGE, {1,0,0,0,1}, 0},
            {CREDITCARD, {1,0,0,0,1}, 0},
            {PROFILE, {1,0,0,0,1}, 0},
            {WATCHES, {1,0,0,0,1}, 0}
        },
        .att = {
            {
                .name = "id",
                .type = 1,0,{0,0,0,0,0}, 0}
        },
        0x40
    },
    { .id = EMAIL, .name = "emailaddress" },
    { .id = PHONE, .name = "phone" },
    { .id = HOMEPAGE, .name = "homepage" },
    { .id = CREDITCARD, .name = "creditcard" },
    {
        .id = ADDRESS,
        .name = "address",
        .elm = {
            {STREET,{1,0,0,1,1}, 0},
            {CITY,{1,0,0,1,1}, 0},
            {COUNTRY,{1,0,0,1,1}, 0},
            {PROVINCE, {1,0,0,0,1}, 0},
            {ZIPCODE,{1,0,0,1,1}, 0}
        }
    },
    { .id = STREET, .name = "street" },
    { .id = CITY, .name = "city" },
    { .id = PROVINCE, .name = "province" },
    { .id = ZIPCODE, .name = "zipcode" },
    { .id = COUNTRY, .name = "country" },
    {
        .id = PROFILE,
        .name = "profile",
        .elm = {
            {INTEREST,{3,3,0,0,25}, 0},
            {EDUCATION, {1,0,0,0,1}, 0},
            {GENDER, {1,0,0,0,1}, 0},
            {BUSINESS,{1,0,0,1,1}, 0},
            {AGE, {1,0,0,0,1}, 0}
        },
        .att = {
            {
                .name = "income",
                .type = 3,0,{0,0,0,0,0},1}
        }
    },
    { .id = EDUCATION, .name = "education" },
    { .id = INCOME, .name = "income" },
    { .id = GENDER, .name = "gender" },
    { .id = BUSINESS, .name = "business" },
    { .id = AGE, .name = "age" },
    {
        .id = INTEREST,
        .name = "interest",
        .elm = {{0,{0,0,0,0,0}, 0}},
        .att = {
            {
                .name = "\1",
                .type = 2,CATEGORY,{1,0,0,0,0}}
        }
    },
    {
        .id = WATCHES,
        .name = "watches",
        .elm = {
            {WATCH,{3,4,0,0,100}, 0},
        }
    },
    {
        .id = WATCH,
        .name = "watch",
        .elm = {{0,{0,0,0,0,0}, 0}},
        .att = {
            {.name = "\1", .type = 2,OPEN_TRANS,{1,0,0,0,0}}
        }
    },
    {
        .id = OPEN_TRANS_LIST,
        .name = "open_auctions",
        .elm = {
            {OPEN_TRANS,{1,0,0,12000,12000}, 0}
        }
    },
    {
        .id = OPEN_TRANS,
        .name = "open_auction",
        .elm = {
            {INIT_PRICE,{1,0,0,1,1}, .rec = 0},
            {RESERVE, {1,0,0,0,1}, .rec = 0},
            {BIDDER,{3,5,0,0,200}, .rec = 0},
            {CURRENT,{1,0,0,1,1}, .rec = 0},
            {PRIVACY, {1,0,0,0,1}, .rec = 0},
            {ITEMREF,{1,0,0,1,1}, .rec = 0},
            {SELLER,{1,0,0,1,1}, .rec = 0},
            {ANNOTATION,{1,0,0,1,1}, .rec = 0},
            {QUANTITY,{1,0,0,1,1}, .rec = 0},
            {TYPE,{1,0,0,1,1}, .rec = 0},
            {INTERVAL,{1,0,0,1,1}, .rec = 0}
        },
        .att = {
            {.name = "id", .type = 1,0,{0,0,0,0,0}}
        },
        0x04|0x40
    },
    { .id = PRIVACY, .name = "privacy" },
    { .id = AMOUNT, .name = "amount" },
    { .id = CURRENT, .name = "current" },
    { .id = INCREASE, .name = "increase" },
    { .id = TYPE, .name = "type" },
    {
        .id = ITEMREF,
        .name = "itemref",
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {.name = "\1", .type = 2,ITEM,{1,0,0,0,0}}
        }
    },
    {
        .id = BIDDER,
        .name = "bidder",
        .elm = {
            {XDATE,{1,0,0,1,1}, .rec = 0},
            {TIME,{1,0,0,1,1}, .rec = 0},
            {PERSONREF,{1,0,0,1,1}, .rec = 0},
            {INCREASE,{1,0,0,1,1}, .rec = 0}
        }
    },
    { .id = TIME, .name = "time" },
    { .id = STATUS, .name = "status" },
    { .id = INIT_PRICE, .name = "initial" },
    {
        .id = PERSONREF,
        .name = "personref",
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {
                .name = "\1",
                .type = 2,
                PERSON,{1,0,0,0,0}
            }
        }
    },
    {
        .id = SELLER,
        .name = "seller",
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {.name = "\1",
            .type = 2,PERSON,{2,0.5,0.10,0,0}}
        }
    },
    {
        .id = INTERVAL,
        .name = "interval",
        .elm = {
            {START,{1,0,0,1,1}, .rec = 0},
            {END,{1,0,0,1,1}, .rec = 0}
        }
    },
    { .id = START, .name = "start" },
    { .id = END, .name = "end" },
    {
        .id = CLOSED_TRANS_LIST,
        .name = "closed_auctions",
        .elm = {
            {CLOSED_TRANS,{1,0,0,3000,3000}, .rec = 0}
        }
    },
    {
        .id = CLOSED_TRANS,
        .name = "closed_auction",
        .elm = {
            {SELLER,{1,0,0,1,1}, .rec = 0},
            {BUYER,{1,0,0,1,1}, .rec = 0},
            {ITEMREF,{1,0,0,1,1}, .rec = 0},
            {PRICE,{1,0,0,1,1}, .rec = 0},
            {XDATE,{1,0,0,1,1}, .rec = 0},
            {QUANTITY,{1,0,0,1,1}, .rec = 0},
            {TYPE,{1,0,0,1,1}, .rec = 0},
            {ANNOTATION,{1,0,0,1,1}, .rec = 0}
        },
        .att = {
            {.name = "",
            .type = 0,0,{0,0,0,0,0}}
        },
        0x04|0x40
    },
    { .id = PRICE, .name = "price" },
    {
        .id = BUYER,
        .name = "buyer",
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {.name = "\1",.type = 2,PERSON,{2,0.5,0.10,0,0}}
        }
    },
    {
        .id = ANNOTATION,
        .name = "annotation",
        .elm = {
            {AUTHOR,{1,0,0,1,1}, .rec = 0},
            {DESCRIPTION,{1,0,0,1,1}, .rec = 0},
            {HAPPINESS,{1,0,0,1,1}, .rec = 0}
        }
    },
    { .id = HAPPINESS, .name = "happiness" },
    {
        .id = AUTHOR,
        .name = "author",
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {.name = "\1",
            .type = 2,PERSON,{1,0,0,1,1}}
        }
    }
};

FILE *OpenOutput_split(const char *global_outputname, int fileno) {
    if (fileno > 99999) {
        fprintf(stderr,"Warning: More than %d files.\n", 99999);
    }
    char *newname = strutil.newstr("%s%0*d", global_outputname, 5, fileno);
    FILE *f = fopen(newname,"w");
    free(newname);
    return f;
}

bool hasID(ObjDesc *od) {
    for (size_t i = 0; i < nelem(od->att); i++) {
        if (od->att[i].type == 0) return false;
        if (od->att[i].type == 1) return true;
    }
    return false;
}

xmlgen_rand.ProbDesc global_GenRef_pdnew = {};
char dtd_name[128]="auction.dtd";

int GenRef(xmlgen_rand.ProbDesc *pd, int type)
{
    ObjDesc* od=objs+type;
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
    return (int)xmlgen_rand.GenRandomNum(&global_GenRef_pdnew);
}
void FixDist(xmlgen_rand.ProbDesc *pd, double val)
{
    pd->min=pd->max=val;
    pd->type=0;
}
void FixReferenceSets(ObjDesc *od)
{
    int i,j,maxref=0;
    if (od->flag++) return;
    for (i=0;i<od->kids;i++)
        {
            ElmDesc *ed=&(od->elm[i]);
            ObjDesc *son=&objs[ed->id];
            if (!son) continue;
            if (ed->pd.min>1 && !hasID(son))
                {
                    int size;
                    double local_factor=1;
                    for (j=0;j<5;j++)
                        {
                            if (son->att[j].name[0]=='\0') break;
                            if (maxref < objs[son->att[j].ref].set.size) {
                                maxref = objs[son->att[j].ref].set.size;
                            }
                        }
                    if (!maxref) break;
                    local_factor=maxref/ed->pd.max;
                    size=(int)(xmlgen_rand.GenRandomNum(&ed->pd)+0.5);
                    if (size*local_factor > 1) {
                        size = (int)(size*local_factor);
                    } else {
                        size = 1;
                    }
                    son->set.size+=size;
                    FixDist(&ed->pd,size);
                }
            FixReferenceSets(son);
        }
}
void FixSetSize(ObjDesc *od) {
    if (od->flag++) {
        return;
    }
    for (int i = 0; i < od->kids; i++) {
        ElmDesc *ed = &(od->elm[i]);
        ObjDesc *son = &objs[ed->id];
        if (!son) continue;
        if (ed->pd.min>1 && (hasID(son) || (son->type&0x04))) {
            int size=(int)(xmlgen_rand.GenRandomNum(&ed->pd)+0.5);
            if (size*global_scale_factor > 1) {
                size = (int)(size*global_scale_factor);
            } else {
                size = 1;
            }
            son->set.size+=size;
            FixDist(&ed->pd,size);
        }
        FixSetSize(son);
    }
}
void FixSetByEdge(char *father_name, char *son_name, int size)
{
    int nobj=nelem(objs);
    for (int i=0; i<nobj; i++) {
        if (strcmp(father_name,objs[i].name)) {
            continue;
        }
        ObjDesc *od=objs+i;
        for (int j=0;j<od->kids;j++) {
            ElmDesc *ed=&(od->elm[j]);
            ObjDesc *son=objs+ed->id;
            if (!strcmp(son_name,son->name)) {
                FixDist(&ed->pd,size);
            }
        }
    }
}

enum {
    ATTR_TYPE_1 = 1,
    ATTR_TYPE_2 = 2,
    ATTR_TYPE_3 = 3
};

void OpeningTag(ObjDesc *od)
{
    AttDesc *att=0;
    stack[stackdepth++]=od;
    xmlprintf(xmlout,"<%s",od->name);
    for (int i=0;i<5;i++) {
        
        att=&od->att[i];
        if (att->name[0]=='\0') {
            break;
        }

        char *attname;
        if (att->name[0]=='\1') {
            attname=objs[att->ref].name;
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
                xmlprintf(xmlout," %s=\"%s%d\"", attname,objs[att->ref].name,ref);
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

void ClosingTag(ObjDesc *od)
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

void GenSubtree(FILE *out, ObjDesc *od)
{
    int i=0;
    ElmDesc *ed;
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
        case CITY:
            ipsum.fcity(out); break;
        case STATUS:
        case HAPPINESS:
            ipsum.fint(out, 1, 10); break;
        case STREET:
            ipsum.fstreet(out); break;
        case PHONE:
            ipsum.fphone(out); break;
        case CREDITCARD:
            ipsum.fcreditcard(out); break;
        case SHIPPING:
            ipsum.fshipping(out); break;
        case TIME:
            ipsum.ftime(out); break;
        case AGE:
            ipsum.fage(out); break;
        case ZIPCODE:
            ipsum.fzipcode(out); break;
        case XDATE:
        case START:
        case END:
            ipsum.fdate(out); break;
        case GENDER:
            ipsum.fgender(out); break;
        case AMOUNT:
        case PRICE:
            ipsum.fprice(out); break;

        case TYPE:
            xmlprintf(out, "%s", GenContents_auction_type[xmlgen_rand.ignuin(0,1)]);
            if (GenContents_quantity>1 && xmlgen_rand.ignuin(0,1)) xmlprintf(out,", Dutch");
            break;
        case LOCATION:
        case COUNTRY:
            if (rnd.uniform(0, 1) < 0.75) {
                GenContents_country = COUNTRIES_USA;
            } else {
                GenContents_country = xmlgen_rand.ignuin(0, words.dictlen("countries") - 1);
            }
            xmlprintf(out, "%s", words.dictentry("countries", GenContents_country));
            break;
        case PROVINCE:
            if (GenContents_country == COUNTRIES_USA) {
                ipsum.fprovince(out);
            } else {
                ipsum.flastname(out);
            }
            break;
        case EDUCATION:            
            xmlprintf(out, "%s", GenContents_education[xmlgen_rand.ignuin(0,3)]);
            break;
        
        case HOMEPAGE:
            xmlprintf(out, "http://www.%s/~%s",
                words.dictentry("emails", GenContents_email),
                words.dictentry("lastnames", GenContents_lstname));
            break;
        
        case PAYMENT:
            r=0;
            for (int i=0;i<4;i++)
                if (xmlgen_rand.ignuin(0,1)) {
                    char *x = "";
                    if (r++) {
                        x = ", ";
                    }
                    xmlprintf(out, "%s%s", x, GenContents_money[i % 4]);
                }
            break;
        
        case BUSINESS:
        case PRIVACY:
            xmlprintf(out,GenContents_yesno[xmlgen_rand.ignuin(0,1)]);
            break;
        
        case CATNAME:
        case ITEMNAME:
            ipsum.fsentence(out, xmlgen_rand.ignuin(1,4));
            break;
        case NAME:
            PrintName();
            break;
        case FROM:
        case TO:
            PrintName();
            xmlprintf(out," ");
            break;
        case EMAIL:
            GenContents_email = xmlgen_rand.ignuin(0, words.dictlen("emails") - 1);
            xmlprintf(out, "mailto:%s@%s",
                words.dictentry("lastnames", GenContents_lstname),
                words.dictentry("emails", GenContents_email));
            break;
        
        case QUANTITY:
            GenContents_quantity=1+(int)rnd.exponential(0.4);
            xmlprintf(out,"%d",GenContents_quantity);
            break;
        case INCREASE:
            double d=1.5 *(1+(int)rnd.exponential(10));
            xmlprintf(out,"%.2f",d);
            GenContents_increases+=d;
        break;
        case CURRENT:
            xmlprintf(out,"%.2f",GenContents_initial+GenContents_increases);
            break;
        case INIT_PRICE:
            GenContents_initial=rnd.exponential(100);
            GenContents_increases=0;
            xmlprintf(out,"%.2f",GenContents_initial);
            break;
        case RESERVE:
            xmlprintf(out,"%.2f",GenContents_initial*(1.2+rnd.exponential(2.5)));
            break;
        case TEXT:
            PrintANY();
            break;
        default:
            has_content = false;
    }
    if (has_content && od->elm[0].id != 0) {
        xmlprintf(out,"\n");
    }

    if (od->type&0x02)
        {
            double sum = 0;
            double alt = rnd.uniform(0, 1);
            i=0;
            if (od->flag>2-1)
                while (i<od->kids-1 && od->elm[i].rec) i++;
            else
                while (i<od->kids-1 && (sum+=od->elm[i].pd.mean)<alt) i++;
            GenSubtree(out, objs+od->elm[i].id);
        }
    else
        for (i=0;i<od->kids;i++)
            {
                int num;
                ed=&od->elm[i];
                num=(int)(xmlgen_rand.GenRandomNum(&ed->pd)+0.5);
                while(num--)
                    GenSubtree(out, objs+ed->id);
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
            xmlprintf(xmlout,"<?xml version=\"1.0\"?>\n<!DOCTYPE %s SYSTEM \"%s\">\n",
                    objs[1].name,dtd_name);
            break;
        case 3:
            xmlprintf(stderr,"Not yet implemented.\n");
            exit(1);
        }
}

void AlignObjs() {
    int nobj = nelem(objs);
    ObjDesc *newobjs = calloc(nobj, sizeof(ObjDesc));
    for (int i = 0; i < nobj; i++) {
        void *dest = &newobjs[objs[i].id];
        void *src = &objs[i];
        memcpy(dest, src, sizeof(ObjDesc));
    }
    memcpy(objs,newobjs,sizeof(ObjDesc)*nobj);
    free(newobjs);
    for (int i = 0; i < nobj; i++)
        for (int j=0; j<20; j++)
            if (objs[i].elm[j].id!=0) objs[i].kids++;
}
int FindRec(ObjDesc *od, ObjDesc *search)
{
    int i,r=0;
    if (od==search) r=1;
    else
        {
            if(!od->flag)
                {
                    od->flag=1;
                    for (i=0;i<od->kids;i++)
                        r+=FindRec(objs+od->elm[i].id,search);
                }
        }
    od->flag=0;
    return r;
}
void CheckRecursion()
{
    int i,j;
    int nobj=nelem(objs);
    ObjDesc *root;
    for (i=1;i<nobj;i++)
        {
            root=objs+i;
            if (!(root->type&0x02)) continue;
            for (j=0;j<root->kids;j++)
                root->elm[j].rec=FindRec(&objs[root->elm[j].id],root);
        }
}

char *dtd[]={
    "<!ELEMENT site            (regions, categories, catgraph, people, open_auctions, closed_auctions)>\n",
    "<!ELEMENT categories      (category+)>\n",
    "<!ELEMENT category        (name, description)>\n",
    "<!ATTLIST category        id ID #REQUIRED>\n",
    "<!ELEMENT name            (#PCDATA)>\n",
    "<!ELEMENT description     (text | parlist)>\n",
    "<!ELEMENT text            (#PCDATA | bold | keyword | emph)*>\n",
    "<!ELEMENT bold		  (#PCDATA | bold | keyword | emph)*>\n",
    "<!ELEMENT keyword	  (#PCDATA | bold | keyword | emph)*>\n",
    "<!ELEMENT emph		  (#PCDATA | bold | keyword | emph)*>\n",
    "<!ELEMENT parlist	  (listitem)*>\n",
    "<!ELEMENT listitem        (text | parlist)*>\n","\n",
    "<!ELEMENT catgraph        (edge*)>\n",
    "<!ELEMENT edge            EMPTY>\n",
    "<!ATTLIST edge            from IDREF #REQUIRED to IDREF #REQUIRED>\n",
    "\n",
    "<!ELEMENT regions         (africa, asia, australia, europe, namerica, samerica)>\n",
    "<!ELEMENT africa          (item*)>\n",
    "<!ELEMENT asia            (item*)>\n",
    "<!ELEMENT australia       (item*)>\n",
    "<!ELEMENT namerica        (item*)>\n",
    "<!ELEMENT samerica        (item*)>\n",
    "<!ELEMENT europe          (item*)>\n",
    "<!ELEMENT item            (location, quantity, name, payment, description, shipping, incategory+, mailbox)>\n",
    "<!ATTLIST item            id ID #REQUIRED\n",
    "                          featured CDATA #IMPLIED>\n",
    "<!ELEMENT location        (#PCDATA)>\n",
    "<!ELEMENT quantity        (#PCDATA)>\n",
    "<!ELEMENT payment         (#PCDATA)>\n",
    "<!ELEMENT shipping        (#PCDATA)>\n",
    "<!ELEMENT reserve         (#PCDATA)>\n",
    "<!ELEMENT incategory      EMPTY>\n",
    "<!ATTLIST incategory      category IDREF #REQUIRED>\n",
    "<!ELEMENT mailbox         (mail*)>\n",
    "<!ELEMENT mail            (from, to, date, text)>\n",
    "<!ELEMENT from            (#PCDATA)>\n",
    "<!ELEMENT to              (#PCDATA)>\n",
    "<!ELEMENT date            (#PCDATA)>\n",
    "<!ELEMENT itemref         EMPTY>\n",
    "<!ATTLIST itemref         item IDREF #REQUIRED>\n",
    "<!ELEMENT personref       EMPTY>\n",
    "<!ATTLIST personref       person IDREF #REQUIRED>\n","\n",
    "<!ELEMENT people          (person*)>\n",
    "<!ELEMENT person          (name, emailaddress, phone?, address?, homepage?, creditcard?, profile?, watches?)>\n",
    "<!ATTLIST person          id ID #REQUIRED>\n",
    "<!ELEMENT emailaddress    (#PCDATA)>\n",
    "<!ELEMENT phone           (#PCDATA)>\n",
    "<!ELEMENT address         (street, city, country, province?, zipcode)>\n",
    "<!ELEMENT street          (#PCDATA)>\n",
    "<!ELEMENT city            (#PCDATA)>\n",
    "<!ELEMENT province        (#PCDATA)>\n",
    "<!ELEMENT zipcode         (#PCDATA)>\n",
    "<!ELEMENT country         (#PCDATA)>\n",
    "<!ELEMENT homepage        (#PCDATA)>\n",
    "<!ELEMENT creditcard      (#PCDATA)>\n",
    "<!ELEMENT profile         (interest*, education?, gender?, business, age?)>\n",
    "<!ATTLIST profile         income CDATA #IMPLIED>\n",
    "<!ELEMENT interest        EMPTY>\n",
    "<!ATTLIST interest        category IDREF #REQUIRED>\n",
    "<!ELEMENT education       (#PCDATA)>\n",
    "<!ELEMENT income          (#PCDATA)>\n",
    "<!ELEMENT gender          (#PCDATA)>\n",
    "<!ELEMENT business        (#PCDATA)>\n",
    "<!ELEMENT age             (#PCDATA)>\n",
    "<!ELEMENT watches         (watch*)>\n",
    "<!ELEMENT watch           EMPTY>\n",
    "<!ATTLIST watch           open_auction IDREF #REQUIRED>\n","\n",
    "<!ELEMENT open_auctions   (open_auction*)>\n",
    "<!ELEMENT open_auction    (initial, reserve?, bidder*, current, privacy?, itemref, seller, annotation, quantity, type, interval)>\n",
    "<!ATTLIST open_auction    id ID #REQUIRED>\n",
    "<!ELEMENT privacy         (#PCDATA)>\n",
    "<!ELEMENT initial         (#PCDATA)>\n",
    "<!ELEMENT bidder          (date, time, personref, increase)>\n",
    "<!ELEMENT seller          EMPTY>\n",
    "<!ATTLIST seller          person IDREF #REQUIRED>\n",
    "<!ELEMENT current         (#PCDATA)>\n",
    "<!ELEMENT increase        (#PCDATA)>\n",
    "<!ELEMENT type            (#PCDATA)>\n",
    "<!ELEMENT interval        (start, end)>\n",
    "<!ELEMENT start           (#PCDATA)>\n",
    "<!ELEMENT end             (#PCDATA)>\n",
    "<!ELEMENT time            (#PCDATA)>\n",
    "<!ELEMENT status          (#PCDATA)>\n",
    "<!ELEMENT amount          (#PCDATA)>\n","\n",
    "<!ELEMENT closed_auctions (closed_auction*)>\n",
    "<!ELEMENT closed_auction  (seller, buyer, itemref, price, date, quantity, type, annotation?)>\n",
    "<!ELEMENT buyer           EMPTY>\n",
    "<!ATTLIST buyer           person IDREF #REQUIRED>\n",
    "<!ELEMENT price           (#PCDATA)>\n",
    "<!ELEMENT annotation      (author, description?, happiness)>\n","\n",
    "<!ELEMENT author          EMPTY>\n",
    "<!ATTLIST author          person IDREF #REQUIRED>\n",
    "<!ELEMENT happiness       (#PCDATA)>\n"
};

int main(int argc, char **argv)
{
    bool dumpdtd = false;
    bool doctype_is_2 = false;
    bool show_version = false;
    bool iflag = false;

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
        for (size_t i = 0; i < nelem(dtd); i++) {
            fprintf(stdout, "%s", dtd[i]);
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

    AlignObjs();
    ObjDesc *root;
    root=objs+1;
    FixSetSize(root);
    for (size_t i = 0; i < nelem(objs); i++) {
        objs[i].flag = 0;
    }
    FixReferenceSets(root);
    for (size_t i = 0; i < nelem(objs); i++) {
        objs[i].flag = 0;
    }
    CheckRecursion();
    for (size_t i = 0; i < nelem(objs); i++) {
        objs[i].flag = 0;
    }
    initialize();
    Preamble(document_type);
    GenSubtree(xmlout, root);
    fclose(xmlout);
    return 0;
}

typedef {
    int cur, out, brosout;
    int max, brosmax;
    int dir, mydir;
    int current;
    xmlgen_rand.random_gen rk;
} idrepro;

idrepro idr[2] = {};

enum {
    ERROR_OBJ,
    AUCTION_SITE,
    CATGRAPH, EDGE, CATEGORY_LIST, CATEGORY, CATNAME,
    PERSON_LIST, PERSON, EMAIL, ADDRESS, PHONE, STREET, CITY, COUNTRY, ZIPCODE,
    PROVINCE, HOMEPAGE, PROFILE, EDUCATION, INCOME, GENDER, BUSINESS, NAME,
    AGE, CREDITCARD, INTEREST,
    REGION, EUROPE, ASIA, NAMERICA, SAMERICA, AFRICA, AUSTRALIA,
    ITEM, LOCATION, QUANTITY, PAYMENT, SHIPPING,
    MAIL, FROM, TO, XDATE, ITEMNAME,
    DESCRIPTION, LISTITEM, PARLIST, TEXT,
    OPEN_TRANS_LIST, OPEN_TRANS, CLOSED_TRANS_LIST, CLOSED_TRANS,
    WATCHES, WATCH, AMOUNT, CURRENT, INCREASE, INTERVAL, RESERVE,
    INCATEGORY, MAILBOX, BIDDER, PRIVACY, ITEMREF, SELLER, TYPE, TIME,
    STATUS, PERSONREF, INIT_PRICE, START, END, BUYER, PRICE, ANNOTATION,
    HAPPINESS, AUTHOR
};

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
                    PrintANY_st[stptr]=xmlgen_rand.ignuin(0,3-1);
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

int ItemIdRef(ObjDesc *odSon, int *iRef)
{
    ObjDesc *od;
    if (odSon->id!=ITEMREF || stackdepth<2) return 0;
    od=stack[stackdepth-2];
    if (od->id==OPEN_TRANS) return GenItemIdRef(&idr[0],iRef);
    if (od->id==CLOSED_TRANS) return GenItemIdRef(&idr[1],iRef);
    return 0;
}
void initialize()
{
    int items = -1;
    int open = -1;
    for (size_t i = 0; i < nelem(objs); i++) {
        if (objs[i].id == ITEM) {
            items = objs[i].set.size;
        }
        if (objs[i].id == OPEN_TRANS) {
            open = objs[i].set.size;
        }
    }
    int closed = items - open;
    FixSetByEdge("closed_auctions","closed_auction",closed);
    InitRepro(&idr[0], open, closed);
    InitRepro(&idr[1], closed, open);
}

int InitRepro_direction=0;
void InitRepro(idrepro *rep, int max, int brosmax)
{
    rep->out=0;
    rep->cur=rep->brosout=0;
    rep->max=max;
    rep->brosmax=brosmax;
    rep->dir=0;
    rep->mydir=InitRepro_direction++;
    rep->cur=0;
}

int GenItemIdRef(idrepro *rep, int *idref)
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
            rep->dir=xmlgen_rand.__ignuin(&rep->rk,0,1);
            while(rep->dir!=rep->mydir && rep->brosout<rep->brosmax)
                {
                    rep->brosout++;
                    rep->cur++;
                    rep->dir=xmlgen_rand.__ignuin(&rep->rk,0,1);
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
