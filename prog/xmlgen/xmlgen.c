#import opt
#import words.c
#import rnd
#import ipsum.c
#import schema.c

typedef {
    schema.ProbDesc genref_pdnew;
} generator_state_t;

generator_state_t GLOBAL_STATE = {};

FILE *xmlout=0;
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
bool GenSubtree_splitnow = false;
char *markup[3]={"emph","keyword","bold"};
char tick[3] = "";
int PrintANY_st[3] = {};

const int COUNTRIES_USA = 0;
schema.Element *stack[64] = {};



int main(int argc, char **argv) {
    bool doctype_is_2 = false;
    float global_scale_factor = 1;

    opt.flag("d", "document_type=2", &doctype_is_2);    
    opt.opt_float("f", "global_scale_factor", &global_scale_factor);
    opt.opt_int("s", "global_split", &global_split);
    opt.parse(argc, argv);

    int document_type=1;
    if (doctype_is_2) {
        document_type=2;
    }
    
    xmlout=stdout;

    schema.InitializeSchema(global_scale_factor);

    Preamble(xmlout, document_type);
    Tree(xmlout, schema.GetSchemaNode(1));
    fclose(xmlout);
    return 0;
}

void Preamble(FILE *out, int document_type) {
    switch (document_type) {
        case 1: {
            fprintf(out, "<?xml version=\"1.0\" standalone=\"yes\"?>\n");
        }
        case 2: {
            fprintf(out, "<?xml version=\"1.0\"?>\n");
            fprintf(out, "<!DOCTYPE %s SYSTEM \"auction.dtd\">\n", schema.GetSchemaNode(1)->name);
        }
        default: { 
            panic("unknown document type: %d", document_type);
        }
    }
}

void Tree(FILE *out, schema.Element *element) {
    if (element->type&0x10) {
        return;
    }
    
    if (GenSubtree_splitnow) {
        // split doc
        int oldstackdepth = stackdepth;
        for (int i = oldstackdepth-1; i>=0; i--) {
            ClosingTag(stack[i]);
        }

        if (xmlout!=stdout) {
            fclose(xmlout);
        }
        
        for (int i=0; i < oldstackdepth; i++) {
            OpeningTag(out, stack[i]);
        }
        splitcnt=0;

        GenSubtree_splitnow = false;
    }

    OpeningTag(out, element);
    
    element->flag++;

    int r;
    bool has_content = true;
    switch(element->id) {
        case schema.CITY: { ipsum.fcity(out); }
        case schema.STATUS,
            schema.HAPPINESS: { ipsum.fint(out, 1, 10); }
        case schema.STREET: { ipsum.fstreet(out); }
        case schema.PHONE: { ipsum.fphone(out); }
        case schema.CREDITCARD: { ipsum.fcreditcard(out); }
        case schema.SHIPPING: { ipsum.fshipping(out); }
        case schema.TIME: { ipsum.ftime(out); }
        case schema.AGE: { ipsum.fage(out); }
        case schema.ZIPCODE: { ipsum.fzipcode(out); }
        case schema.XDATE, schema.START, schema.END: { ipsum.fdate(out); }
        case schema.GENDER: { ipsum.fgender(out); }
        case schema.AMOUNT,
            schema.PRICE: { ipsum.fprice(out); }

        case schema.TYPE: {
            fprintf(out, "%s", GenContents_auction_type[rnd.intn(2)]);
            if (GenContents_quantity > 1 && rnd.intn(2) > 0) {
				fprintf(out,", Dutch");
			}
        }
        case schema.LOCATION, schema.COUNTRY: {
            if (rnd.urange(0, 1) < 0.75) {
                GenContents_country = COUNTRIES_USA;
            } else {
                GenContents_country = rnd.intn(words.dictlen("countries"));
            }
            fprintf(out, "%s", words.dictentry("countries", GenContents_country));
        }
        case schema.PROVINCE: {
            if (GenContents_country == COUNTRIES_USA) {
                ipsum.fprovince(out);
            } else {
                ipsum.flastname(out);
            }
        }
        case schema.EDUCATION: {
            fprintf(out, "%s", GenContents_education[rnd.intn(4)]);
        }
        case schema.HOMEPAGE: {
            fprintf(out, "http://www.%s/~%s",
                words.dictentry("emails", GenContents_email),
                words.dictentry("lastnames", GenContents_lstname));
        }
        case schema.PAYMENT: {
            r=0;
            for (int i=0; i<4; i++) {
                if (rnd.intn(2)) {
                    char *x = "";
                    if (r++) {
                        x = ", ";
                    }
                    fprintf(out, "%s%s", x, GenContents_money[i % 4]);
                }
            }
        }
        case schema.BUSINESS, schema.PRIVACY: {
            fprintf(out, "%s", GenContents_yesno[rnd.intn(2)]);
        }
        case schema.CATNAME, schema.ITEMNAME: {
            ipsum.fsentence(out, 1 + rnd.intn(4));
        }
        case schema.NAME: { PrintName(); }
        case schema.FROM, schema.TO: {
            PrintName();
            fprintf(out," ");
        }
        case schema.EMAIL: {
            GenContents_email = rnd.intn(words.dictlen("emails"));
            fprintf(out, "mailto:%s@%s",
                words.dictentry("lastnames", GenContents_lstname),
                words.dictentry("emails", GenContents_email));
        }
        case schema.QUANTITY: {
            GenContents_quantity=1+(int)rnd.exponential(0.4);
            fprintf(out,"%d",GenContents_quantity);
        }
        case schema.INCREASE: {
            double d=1.5 *(1+(int)rnd.exponential(10));
            fprintf(out,"%.2f",d);
            GenContents_increases+=d;
        }
        case schema.CURRENT: {
            fprintf(out,"%.2f",GenContents_initial+GenContents_increases);
        }
        case schema.INIT_PRICE: {
            GenContents_initial=rnd.exponential(100);
            GenContents_increases=0;
            fprintf(out,"%.2f",GenContents_initial);
        }
        case schema.RESERVE: {
            fprintf(out,"%.2f",GenContents_initial*(1.2+rnd.exponential(2.5)));
        }
        case schema.TEXT: { PrintANY(out); }
        default: {
            has_content = false;
        }
    }
    if (has_content && element->elm[0] != 0) {
        fprintf(out,"\n");
    }

    if (element->type & 0x02) {
        schema.Element *root;
        double alt = rnd.urange(0, 1);
        double sum = 0;
        int *child_id = element->elm;            
        while (*child_id) {
            schema.ProbDesc pd = schema.probDescForChild(element, *child_id);
            sum += pd.mean;
            if (sum >= alt) {
                root = schema.GetSchemaNode(*child_id);
                break;
            }
            child_id++;
        }
        if (!root) {
            panic("failed to determine root");
        }
        Tree(out, root);
    }
    else {
        int *child_id = element->elm;
        while (*child_id) {
            schema.ProbDesc pd = schema.probDescForChild(element, *child_id);
            int num = (int)(GenRandomNum(&pd) + 0.5);
            for (int i = 0; i < num; i++) {
                Tree(out, schema.GetSchemaNode(*child_id));
            }
            child_id++;
        }
    }
    ClosingTag(element);
    if (global_split) {
        if (element->type & 0x20 || (element->type & 0x40 && splitcnt++>global_split)) {
            GenSubtree_splitnow = true;
        }
    }
    element->flag--;
}

void OpeningTag(FILE *out, schema.Element *element) {
    fprintf(out, "<%s", element->name);

    stack[stackdepth++] = element;

    for (int i=0; i<5; i++) {
        schema.AttDesc *att = &element->att[i];
        if (att->name[0] == '\0') {
            break;
        }

        char *attname;
        if (att->name[0] == '\1') {
            attname = schema.GetSchemaNode(att->ref)->name;
        } else {
            attname = att->name;
        }

        switch (att->type) {
            case schema.ATTR_TYPE_1: {
                fprintf(out, " %s=\"%s%d\"", attname, element->name, element->set.id++);
            }
            case schema.ATTR_TYPE_2: {
                int ref=0;
                if (!ItemIdRef(element, &ref)) {
                    schema.ProbDesc pd = schema.probDescForAttr(element->id, att->name);
                    int element_id = att->ref;
                    schema.Element* element = schema.GetSchemaNode(element_id);
                    if (pd.type != 0) {
                        GLOBAL_STATE.genref_pdnew.type = pd.type;
                        GLOBAL_STATE.genref_pdnew.min = 0;
                        GLOBAL_STATE.genref_pdnew.max = element->set.size - 1;
                        if (pd.type != 1) {
                            GLOBAL_STATE.genref_pdnew.mean = pd.mean * GLOBAL_STATE.genref_pdnew.max;
                            GLOBAL_STATE.genref_pdnew.dev = pd.dev * GLOBAL_STATE.genref_pdnew.max;
                        }
                    }
                    ref = (int) GenRandomNum(&GLOBAL_STATE.genref_pdnew);
                }
                fprintf(out," %s=\"%s%d\"", attname, schema.GetSchemaNode(att->ref)->name, ref);
            }
            case schema.ATTR_TYPE_3: {
                if (rnd.urange(0, 1) < att->prcnt) {
                    if (!strcmp(attname,"income")) {
                        double d = 40000 + 30000 * rnd.gauss();
                        if (d < 9876) {
                            d = 9876;
                        }
                        fprintf(out," %s=\"%.2f\"",attname, d);
                    } else {
                        fprintf(out," %s=\"yes\"",attname);
                    }
                }
            }
            default: {
                fflush(out);
                panic("unknown ATT type %s\n", attname);
            }
        }
    }
    if (element->elm[0] == 0 && (element->att[0].name[0])) {
        fprintf(out,"/>\n");
    } else {
        fprintf(out,">");
        if (element->elm[0] != 0 || element->type & 0x01) {
            fprintf(out,"\n");
        }
    }
}

void ClosingTag(schema.Element *element) {
    stackdepth--;
    if (element->type & 0x01) {
        fprintf(xmlout,"\n");
    }
    if ((element->att[0].name[0]) && element->elm[0] == 0) {
        return;
    }
    fprintf(xmlout,"</%s>\n",element->name);
}



void PrintANY(FILE *out) {
    int sen=1+(int)rnd.exponential(20);
    int stptr = 0;
    for (int i=0; i < sen; i++) {
        if (rnd.urange(0, 1) < 0.1 && stptr < 2) {
            while (true) {
                PrintANY_st[stptr] = rnd.intn(3);
                if (!tick[PrintANY_st[stptr]]) {
                    break;
                }
            }
            tick[PrintANY_st[stptr]] = 1;
            fprintf(out,"<%s> ", markup[PrintANY_st[stptr]]);
            stptr++;
        }
        else if (rnd.urange(0, 1) < 0.8 && stptr != 0) {
            --stptr;
            fprintf(out,"</%s> ",markup[PrintANY_st[stptr]]);
            tick[PrintANY_st[stptr]] = 0;
        }
        ipsum.fsentence(out, 1 + (int)rnd.exponential(4));
    }
    while (stptr) {
        --stptr;
        fprintf(out,"</%s> ",markup[PrintANY_st[stptr]]);
        tick[PrintANY_st[stptr]]=0;
    }
}

int ItemIdRef(schema.Element *child, int *iRef) {
    if (child->id != schema.ITEMREF || stackdepth < 2) {
        return 0;
    }
    schema.Element *element = stack[stackdepth-2];
    if (element->id == schema.OPEN_TRANS) {
        return GenItemIdRef(schema.getidr(0), iRef);
    }
    if (element->id == schema.CLOSED_TRANS) {
        return GenItemIdRef(schema.getidr(1), iRef);
    }
    return 0;
}

int GenItemIdRef(schema.idrepro *rep, int *idref) {
    if (rep->out >= rep->max) {
        return 0;
    }

    rep->out++;
    if (rep->brosout >= rep->brosmax) {
        *idref = rep->cur++;
        return 2;
    }

    rep->dir = rnd.intn(2);
    while (rep->dir != rep->mydir && rep->brosout < rep->brosmax) {
        rep->brosout++;
        rep->cur++;
        rep->dir = rnd.intn(2);
    }
    *idref = rep->cur++;
    return 1;
}

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
        return rnd.urange(pd->min, pd->max);
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
