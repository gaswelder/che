#import xmlgen_rand.c
#import strutil

typedef {
    int id;
    ProbDesc pd;
    char rec;
} ElmDesc;

typedef {
    char name[20];
    int type;
    int ref;
    ProbDesc pd;
    float prcnt;
} AttDesc;

typedef {
    int size,id;
    SetDesc *next;
} SetDesc;

typedef {
    int id;
    char *name;
    ElmDesc elm[20];
    AttDesc att[5];
    int type;
    int kids;
    SetDesc set;
    int flag;
} ObjDesc;

FILE *xmlout=0;
char *outputname=0;
int indent_inc=0;
double scale_factor=1;
ObjDesc *stack[64] = {};
int stackdepth=0;
int stackatt=0;
int split=0;
int splitcnt=0;

typedef int printfunc_t(FILE *, const char *, ...);

printfunc_t *xmlprintf = &fprintf;

int global_split_fileno = 0;

FILE *OpenOutput_split(const char *outputname, int fileno) {
    if (fileno > 99999) {
        fprintf(stderr,"Warning: More than %d files.\n", 99999);
    }
    char *newname = newstr("%s%0*d", outputname, 5, fileno);
    FILE *f = fopen(newname,"w");
    free(newname);
    return f;
}

bool hasID(ObjDesc *od) {
    for (int i=0;i<5;i++) {
        if (od->att[i].type == 0) break;
        if (od->att[i].type == 1) return true;
    }
    return false;
}

ProbDesc global_GenRef_pdnew = {};

int GenRef(ProbDesc *pd, int type)
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
    return (int)GenRandomNum(&global_RenRef_pdnew);
}
void FixDist(ProbDesc *pd, double val)
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
                    size=(int)(GenRandomNum(&ed->pd)+0.5);
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
void FixSetSize(ObjDesc *od)
{
    int i;
    if (od->flag++) return;
    for (i=0;i<od->kids;i++)
        {
            ObjDesc *son;
            ElmDesc *ed=&(od->elm[i]);
            son=&objs[ed->id];
            if (!son) continue;
            if (ed->pd.min>1 && (hasID(son) || (son->type&0x04)))
                {
                    int size=(int)(GenRandomNum(&ed->pd)+0.5);
                    if (size*scale_factor > 1) {
                        size = (int)(size*scale_factor);
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
    int nobj=NumberOfObjs();
    int i,j,fixed=0;
    for (i=0;i<nobj;i++)
        {
            if (!strcmp(father_name,objs[i].name))
                {
                    ObjDesc *od=objs+i;
                    for (j=0;j<od->kids;j++)
                        {
                            ElmDesc *ed=&(od->elm[j]);
                            ObjDesc *son=objs+ed->id;
                            if (!strcmp(son_name,son->name))
                                {
                                    FixDist(&ed->pd,size);
                                    fixed=1;
                                }
                        }
                }
        }
}
void ClearFlags()
{
    int i;
    int nobj=NumberOfObjs();
    for(i=0;i<nobj;i++)
        objs[i].flag=0;
}

char global_OpeningTag_cdata[1024] = "yes";

void OpeningTag(ObjDesc *od)
{
    int i;
    AttDesc *att=0;
    stack[stackdepth++]=od;
    xmlprintf(xmlout,"<%s",od->name);
    for (i=0;i<5;i++)
        {
            char *attname;
            att=&od->att[i];
            if (att->name[0]=='\0') break;
            if (att->name[0]=='\1') attname=objs[att->ref].name;
            else attname=att->name;
            switch(att->type) {
                case 1:
                    xmlprintf(xmlout," %s=\"%s%d\"",
                              attname,od->name,od->set.id++);
                    break;
                case 2:
                    int ref=0;
                    if (!ItemIdRef(od,att->ref,&ref)) {
                        ref=GenRef(&att->pd,att->ref);
                    }
                    xmlprintf(xmlout," %s=\"%s%d\"", attname,objs[att->ref].name,ref);
                break;
                case 3:
                    if (genunf(0,1)<att->prcnt)
                        {
                            GenAttCDATA(od,attname,global_OpeninTag_cdata);
                            xmlprintf(xmlout," %s=\"%s\"",attname,global_OpeninTag_cdata);
                        }
                    break;
                default:
                    fflush(xmlout);
                    fprintf(stderr,"unknown ATT type %s\n",attname);
                    exit(EXIT_FAILURE);
                }
        }
    if (!(od->elm[0].id!=0) && (od->att[0].name[0])) xmlprintf(xmlout,"/>\n");
    else
        {
            xmlprintf(xmlout,">");
            if ((od->elm[0].id!=0) || od->type&0x01) xmlprintf(xmlout,"\n");
        }
}
void ClosingTag(ObjDesc *od)
{
    stackdepth--;
    if (od->type&0x01) xmlprintf(xmlout,"\n");
    if ((od->att[0].name[0]) && !(od->elm[0].id!=0)) return;
    xmlprintf(xmlout,"</%s>\n",od->name);
}

void SplitDoc() {
    int i;
    int oldstackdepth=stackdepth;
    for (int i = oldstackdepth-1; i>=0; i--) {
        indent_level -= indent_inc;
        ClosingTag(stack[i]);
    }

    if (xmlout!=stdout) {
        fclose(xmlout);
    }
    if (outputname) {
        xmlout = OpenOutput_split(outputname, global_split_fileno++);
        if (!xmlout) {
            fflush(stdout);
            fprintf(stderr, "Can't open file %s\n", outputname);
            exit(EXIT_FAILURE);
        }
    }
    for (int i=0; i<oldstackdepth; i++) {
        OpeningTag(stack[i]);
        indent_level+=indent_inc;
    }
    splitcnt=0;
}

bool GenSubtree_splitnow = false;

void GenSubtree(ObjDesc *od)
{
    int i=0;
    ElmDesc *ed;
    if (od->type&0x10) return;
    if (GenSubtree_splitnow) {
        SplitDoc();
        GenSubtree_splitnow = false;
    }
    OpeningTag(od);
    indent_level+=indent_inc;
    od->flag++;
    if (GenContents(xmlout, od->id) && (od->elm[0].id != 0)) {
        xmlprintf(xmlout,"\n");
    }
    if (od->type&0x02)
        {
            double sum=0,alt=genunf(0,1);
            i=0;
            if (od->flag>2-1)
                while (i<od->kids-1 && od->elm[i].rec) i++;
            else
                while (i<od->kids-1 && (sum+=od->elm[i].pd.mean)<alt) i++;
            GenSubtree(objs+od->elm[i].id);
        }
    else
        for (i=0;i<od->kids;i++)
            {
                int num;
                ed=&od->elm[i];
                num=(int)(GenRandomNum(&ed->pd)+0.5);
                while(num--)
                    GenSubtree(objs+ed->id);
            }
    indent_level-=indent_inc;
    ClosingTag(od);
    if (split && (od->type&0x20 || (od->type&0x40 && splitcnt++>split))) {
        GenSubtree_splitnow = true;
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
            exit(EXIT_FAILURE);
        }
}
void Version()
{
    fprintf(stderr,"This is xmlgen, version %s.%s\n%s\n","0","92","by Florian Waas (flw@mx4.org)");
}
void Usage(char *progname)
{
    Version();
    fprintf(stderr, "Usage: %s [ %ch ] [ %cditve ] [ %cf <factor> ] [ %co <file> ] [ %cs <cnt> ]\n",progname,dash,dash,dash,dash,dash);
    exit(EXIT_FAILURE);
}

void AlignObjs()
{
    int i=0,j;
    ObjDesc * newobjs;
    int nobj=NumberOfObjs();
    newobjs=(ObjDesc*)malloc(sizeof(ObjDesc)*nobj);
    memset(newobjs,0,sizeof(ObjDesc)*nobj);
    for (i=0;i<nobj;i++)
        memcpy(&newobjs[objs[i].id], &objs[i],sizeof(ObjDesc));
    memcpy(objs,newobjs,sizeof(ObjDesc)*nobj);
    free(newobjs);
    for (i=0;i<nobj;i++)
        for (j=0;j<20;j++)
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
    int nobj=NumberOfObjs();
    ObjDesc *root;
    for (i=1;i<nobj;i++)
        {
            root=objs+i;
            if (!(root->type&0x02)) continue;
            for (j=0;j<root->kids;j++)
                root->elm[j].rec=FindRec(&objs[root->elm[j].id],root);
        }
}
void printdtd()
{
    int i;
    for (i=0;i<dtd_len;i++)
        fprintf(xmlout,dtd[i]);
}
int main(int argc, char **argv)
{
    int opt,stop=0,timing=0,dumpdtd=0;
    int document_type=1;
    ObjDesc *root;
    if (argc==1) Usage(argv[0]);
    xmlout=stdout;
    while((opt=pmgetopt(argc,argv,
                      "edf:o:ihvs:tw:"
        ))!=-1)
        {
            switch(opt)
                {
                case 'e':
                    dumpdtd=1;
                    break;
                case 'f':
                    scale_factor=atof(pmoptarg);
                    break;
                case 'o':
                    outputname=(char*)malloc(strlen(pmoptarg)+1);
                    strcpy(outputname,pmoptarg);
                    break;
                case 's':
                    split=atoi(pmoptarg);
                    break;
                case 'd':
                    document_type=2;
                    break;
                case 'i':
                    indent_inc=2;
                    xmlprintf=xmlfmtprintf;
                    break;
                case 'v':
                    Version();
                    stop=1;
                    break;
                case 't':
                    timing=1;
                    break;
                case 'w':
                    fmt_width=atoi(pmoptarg);
                    break;
                default:
                    Usage(argv[0]);
                }
        }
    if (stop) exit(EXIT_SUCCESS);
    if (timing) timediff();
    if (xmlout!=stdout) {
        fclose(xmlout);
    }
    if (outputname) {
        if (split) {
            xmlout = OpenOutput_split(outputname, global_split_fileno++);
        } else {
            xmlout = fopen(outputname, "w");
        }
        if (!xmlout) {
            fflush(stdout);
            fprintf(stderr, "Can't open file %s\n", outputname);
            exit(EXIT_FAILURE);
        }
    }
    if (dumpdtd) {
        printdtd();
        fclose(xmlout);
        if (timing) {
            fprintf(stderr,"Elapsed time: %.3f sec\n",timediff()/1E6);
        }
        return 0;
    }
    AlignObjs();
    root=objs+1;
    FixSetSize(root);
    ClearFlags();
    FixReferenceSets(root);
    ClearFlags();
    CheckRecursion();
    ClearFlags();
    initialize();
    Preamble(document_type);
    GenSubtree(root);
    fclose(xmlout);
    if (timing)
        fprintf(stderr,"Elapsed time: %.3f sec\n",timediff()/1E6);
    return 0;
}

typedef {
    int cur, out, brosout;
    int max, brosmax;
    int dir, mydir;
    int current;
    random_gen rk;
} idrepro;

idrepro idr[2] = {};
char dtd_name[128]="auction.dtd";
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
ObjDesc objs[]={
    {
        0, "*error*"
    },
    {
        AUCTION_SITE, "site",
        {
            {REGION,{1,0,0,1,1}},
            {CATEGORY_LIST,{1,0,0,1,1}},
            {CATGRAPH,{1,0,0,1,1}},
            {PERSON_LIST,{1,0,0,1,1}},
            {OPEN_TRANS_LIST,{1,0,0,1,1}},
            {CLOSED_TRANS_LIST,{1,0,0,1,1}}
        }
    },
    {
        CATEGORY_LIST, "categories",
        {
            {CATEGORY,{1,0,0,1000,1000}}
        }
    },
    {
        REGION, "regions",
        {
            {AFRICA,{1,0,0,1,1}},
            {ASIA,{1,0,0,1,1}},
            {AUSTRALIA,{1,0,0,1,1}},
            {EUROPE,{1,0,0,1,1}},
            {NAMERICA,{1,0,0,1,1}},
            {SAMERICA,{1,0,0,1,1}}
        }
    },
    {
        EUROPE, "europe",
        {
            {ITEM,{1,0,0,6000,6000}}
        }
    },
    {
        AUSTRALIA, "australia",
        {
            {ITEM,{1,0,0,2200,2200}}
        }
    },
    {
        AFRICA, "africa",
        {
            {ITEM,{1,0,0,550,550}}
        }
    },
    {
        NAMERICA, "namerica",
        {
            {ITEM,{1,0,0,10000,10000}}
        }
    },
    {
        SAMERICA, "samerica",
        {
            {ITEM,{1,0,0,1000,1000}}
        }
    },
    {
        ASIA, "asia",
        {
            {ITEM,{1,0,0,2000,2000}}
        }
    },
    {
        CATGRAPH, "catgraph",
        {
            {EDGE,{1,0,0,3800,3800}}
        },
        {{"",0,0,{0,0,0,0,0}}},
        0x20
    },
    {
        EDGE, "edge",
        {{0,{0,0,0,0,0}}},
        {
            {"from",2,CATEGORY,{1,0,0,0,0}},
            {"to",2,CATEGORY,{1,0,0,0,0}}
        }
    },
    {
        CATEGORY, "category",
        {
            {CATNAME,{1,0,0,1,1}},
            {DESCRIPTION,{1,0,0,1,1}},
        },
        {
            {"id",1,0,{0,0,0,0,0}}
        }
    },
    {
        ITEM, "item",
        {
            {LOCATION,{1,0,0,1,1}},
            {QUANTITY,{1,0,0,1,1}},
            {ITEMNAME,{1,0,0,1,1}},
            {PAYMENT,{1,0,0,1,1}},
            {DESCRIPTION,{1,0,0,1,1}},
            {SHIPPING,{1,0,0,1,1}},
            {INCATEGORY,{3,3,0,1,10}},
            {MAILBOX,{1,0,0,1,1}}
        },
        {
            {"id",1,0,{0,0,0,0,0}},
            {"featured",3,0,{0,0,0,0,0},0.1}
        },
        0x40
    },
    {
        LOCATION, "location"
    },
    {
        QUANTITY, "quantity"
    },
    {
        PAYMENT, "payment"
    },
    {
        NAME, "name"
    },
    {
        ITEMNAME, "name"
    },
    {
        CATNAME, "name"
    },
    {
        DESCRIPTION, "description",
        {
            {TEXT,{1,0.7,0,0,0}},
            {PARLIST,{1,0.3,0,0,0}}
        },
        {{"",0,0,{0,0,0,0,0}}},
        0x02
    },
    {
        PARLIST, "parlist",
        {
            {LISTITEM,{3,1,0,2,5}}
        }
    },
    {
        TEXT, "text",
        {{0,{0,0,0,0,0}}},
        {{"",0,0,{0,0,0,0,0}}},
        0x01
    },
    {
        LISTITEM, "listitem",
        {
            {TEXT,{1,0.8,0,0,0}},
            {PARLIST,{1,0.2,0,0,0}}
        },
        {{"",0,0,{0,0,0,0,0}}},
        0x02
    },
    {
        SHIPPING, "shipping"
    },
    {
        RESERVE, "reserve"
    },
    {
        INCATEGORY, "incategory",
        {{0,{0,0,0,0,0}}},
        {
            {"\1",2,CATEGORY,{1,0,0,0,0}}
        }
    },
    {
        MAILBOX, "mailbox",
        {
            {MAIL,{3,1,0,0,250}}
        }
    },
    {
        MAIL, "mail",
        {
            {FROM,{1,0,0,1,1}},
            {TO,{1,0,0,1,1}},
            {XDATE,{1,0,0,1,1}},
            {TEXT,{1,0,0,1,1}}
        }
    },
    {
        FROM, "from"
    },
    {
        TO, "to"
    },
    {
        XDATE, "date"
    },
    {
        PERSON_LIST, "people",
        {
            {PERSON,{1,0,0,25500,25500}}
        }
    },
    {
        PERSON, "person",
        {
            {NAME,{1,0,0,1,1}},
            {EMAIL,{1,0,0,1,1}},
            {PHONE, {1,0,0,0,1}},
            {ADDRESS, {1,0,0,0,1}},
            {HOMEPAGE, {1,0,0,0,1}},
            {CREDITCARD, {1,0,0,0,1}},
            {PROFILE, {1,0,0,0,1}},
            {WATCHES, {1,0,0,0,1}}
        },
        {
            {"id",1,0,{0,0,0,0,0}}
        },
        0x40
    },
    {
        EMAIL, "emailaddress"
    },
    {
        PHONE, "phone"
    },
    {
        HOMEPAGE, "homepage"
    },
    {
        CREDITCARD, "creditcard"
    },
    {
        ADDRESS, "address",
        {
            {STREET,{1,0,0,1,1}},
            {CITY,{1,0,0,1,1}},
            {COUNTRY,{1,0,0,1,1}},
            {PROVINCE, {1,0,0,0,1}},
            {ZIPCODE,{1,0,0,1,1}}
        }
    },
    {
        STREET, "street"
    },
    {
        CITY, "city"
    },
    {
        PROVINCE, "province"
    },
    {
        ZIPCODE, "zipcode"
    },
    {
        COUNTRY, "country"
    },
    {
        PROFILE, "profile",
        {
            {INTEREST,{3,3,0,0,25}},
            {EDUCATION, {1,0,0,0,1}},
            {GENDER, {1,0,0,0,1}},
            {BUSINESS,{1,0,0,1,1}},
            {AGE, {1,0,0,0,1}}
        },
        {
            {"income",3,0,{0,0,0,0,0},1}
        }
    },
    {
        EDUCATION, "education"
    },
    {
        INCOME, "income"
    },
    {
        GENDER, "gender"
    },
    {
        BUSINESS, "business"
    },
    {
        AGE, "age"
    },
    {
        INTEREST, "interest",
        {{0,{0,0,0,0,0}}},
        {
            {"\1",2,CATEGORY,{1,0,0,0,0}}
        }
    },
    {
        WATCHES, "watches",
        {
            {WATCH,{3,4,0,0,100}},
        }
    },
    {
        WATCH, "watch",
        {{0,{0,0,0,0,0}}},
        {
            {"\1",2,OPEN_TRANS,{1,0,0,0,0}}
        }
    },
    {
        OPEN_TRANS_LIST, "open_auctions",
        {
            {OPEN_TRANS,{1,0,0,12000,12000}}
        }
    },
    {
        OPEN_TRANS, "open_auction",
        {
            {INIT_PRICE,{1,0,0,1,1}},
            {RESERVE, {1,0,0,0,1}},
            {BIDDER,{3,5,0,0,200}},
            {CURRENT,{1,0,0,1,1}},
            {PRIVACY, {1,0,0,0,1}},
            {ITEMREF,{1,0,0,1,1}},
            {SELLER,{1,0,0,1,1}},
            {ANNOTATION,{1,0,0,1,1}},
            {QUANTITY,{1,0,0,1,1}},
            {TYPE,{1,0,0,1,1}},
            {INTERVAL,{1,0,0,1,1}}
        },
        {
            {"id",1,0,{0,0,0,0,0}}
        },
        0x04|0x40
    },
    {
        PRIVACY, "privacy"
    },
    {
        AMOUNT, "amount"
    },
    {
        CURRENT, "current"
    },
    {
        INCREASE, "increase"
    },
    {
        TYPE, "type"
    },
    {
        ITEMREF, "itemref",
        {{0,{0,0,0,0,0}}},
        {
            {"\1",2,ITEM,{1,0,0,0,0}}
        }
    },
    {
        BIDDER, "bidder",
        {
            {XDATE,{1,0,0,1,1}},
            {TIME,{1,0,0,1,1}},
            {PERSONREF,{1,0,0,1,1}},
            {INCREASE,{1,0,0,1,1}}
        }
    },
    {
        TIME, "time"
    },
    {
        STATUS, "status",
    },
    {
        INIT_PRICE, "initial"
    },
    {
        PERSONREF, "personref",
        {{0,{0,0,0,0,0}}},
        {
            {"\1",2,PERSON,{1,0,0,0,0}}
        }
    },
    {
        SELLER, "seller",
        {{0,{0,0,0,0,0}}},
        {
            {"\1",2,PERSON,{2,0.5,0.10,0,0}}
        }
    },
    {
        INTERVAL, "interval",
        {
            {START,{1,0,0,1,1}},
            {END,{1,0,0,1,1}}
        }
    },
    {
        START, "start"
    },
    {
        END, "end"
    },
    {
        CLOSED_TRANS_LIST, "closed_auctions",
        {
            {CLOSED_TRANS,{1,0,0,3000,3000}}
        }
    }
    ,
    {
        CLOSED_TRANS, "closed_auction",
        {
            {SELLER,{1,0,0,1,1}},
            {BUYER,{1,0,0,1,1}},
            {ITEMREF,{1,0,0,1,1}},
            {PRICE,{1,0,0,1,1}},
            {XDATE,{1,0,0,1,1}},
            {QUANTITY,{1,0,0,1,1}},
            {TYPE,{1,0,0,1,1}},
            {ANNOTATION,{1,0,0,1,1}}
        },
        {{"",0,0,{0,0,0,0,0}}},
        0x04|0x40
    },
    {
        PRICE, "price"
    },
    {
        BUYER, "buyer",
        {{0,{0,0,0,0,0}}},
        {
            {"\1",2,PERSON,{2,0.5,0.10,0,0}}
        }
    },
    {
        ANNOTATION, "annotation",
        {
            {AUTHOR,{1,0,0,1,1}},
            {DESCRIPTION,{1,0,0,1,1}},
            {HAPPINESS,{1,0,0,1,1}}
        }
    },
    {
        HAPPINESS, "happiness"
    },
    {
        AUTHOR, "author",
        {{0,{0,0,0,0,0}}},
        {
            {"\1",2,PERSON,{1,0,0,1,1}}
        }
    }
};
void PrintName(int *lastout)
{
    int fst=(int)genexp(firstnames_len/3);
    int lst=(int)genexp(lastnames_len/3);

    if (fst >= firstnames_len-1) {
        fst = firstnames_len-1;
    }
    if (lst >= lastnames_len-1) {
        lst = lastnames_len-1;
    }
    xmlprintf(xmlout,"%s %s",firstnames[fst],lastnames[lst]);
    if (lastout) *lastout=lst;
}
void PrintEmail()
{
    int i,j;
    i=ignuin(0,firstnames_len-1);
    j=ignuin(0,lastnames_len-1);
    xmlprintf(xmlout,"%s@%s.com",firstnames[i],lastnames[j]);
}
void PrintSentence(int w) {
    for (int i=0; i<w; i++) {
        int word=(int)genexp(words_len/5.0);
        if (word >= words_len - 1) {
            word = words_len - 1;
        }
        xmlprintf(xmlout,words[word]);
        xmlprintf(xmlout," ");
    }
}
char *markup[3]={"emph","keyword","bold"};
char tick[3] = "";

int PrintANY_st[3] = {};

void PrintANY() {
    int sen=1+(int)genexp(20);
    int stptr=0;
    for (int i=0;i<sen;i++)
        {
            if (genunf(0,1)<0.1 && stptr<3-1)
            {
                while (true) {
                    PrintANY_st[stptr]=ignuin(0,3-1);
                    if (!tick[PrintANY_st[stptr]]) {
                        break;
                    }
                }
                tick[PrintANY_st[stptr]]=1;
                xmlprintf(xmlout,"<%s> ",markup[PrintANY_st[stptr]]);
                stptr++;
            }
            else
                if (genunf(0,1)<0.8 && stptr)
                    {
                        --stptr;
                        xmlprintf(xmlout,"</%s> ",markup[PrintANY_st[stptr]]);
                        tick[PrintANY_st[stptr]]=0;
                    }
            PrintSentence(1+(int)genexp(4));
       }
    while(stptr)
        {
            --stptr;
            xmlprintf(xmlout,"</%s> ",markup[PrintANY_st[stptr]]);
            tick[PrintANY_st[stptr]]=0;
        }
}




int NumberOfObjs()
{
    return (sizeof(objs) / sizeof(*objs));
}
int ItemIdRef(ObjDesc *odSon, int type, int *iRef)
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
    int nobj=NumberOfObjs();
    int search[3]={ITEM,OPEN_TRANS,CLOSED_TRANS};
    int f[3]={0,0,0},items,open,closed;
    int i,j;
    for (i=0;i<nobj;i++)
        for (j=0;j<3;j++)
            if (objs[i].id==search[j]) f[j]=i;
    items=objs[f[0]].set.size;
    open=objs[f[1]].set.size;
    closed=items-open;
    FixSetByEdge("closed_auctions","closed_auction",closed);
    InitReproPair(&idr[0],&idr[1],open,closed);
}
void GenAttCDATA(ObjDesc *od, char *attName, char *cdata)
{
    if (!strcmp(attName,"income")) {
        double d = gennor(40000,30000);
        if (d < 9876) {
            d = 9876;
        }
        sprintf(cdata,"%.2f",d);
    }
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

void InitReproPair(idrepro *rep1, idrepro *rep2,
                   int max1, int max2)
{
    InitRepro(rep1,max1,max2);
    InitRepro(rep2,max2,max1);
}
int GenItemIdRef(idrepro *rep, int *idref)
{
    int res=0;
    if (rep->out>=rep->max) return 0;
    rep->out++;
    if (rep->brosout>=rep->brosmax)
        {
            *idref=rep->cur++;
            res=2;
        }
    else
        {
            rep->dir=__ignuin(&rep->rk,0,1);
            while(rep->dir!=rep->mydir && rep->brosout<rep->brosmax)
                {
                    rep->brosout++;
                    rep->cur++;
                    rep->dir=__ignuin(&rep->rk,0,1);
                }
            *idref=rep->cur++;
            res=1;
        }
    return res;
}

