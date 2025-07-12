#import rnd

char *dtd[] = {
    "<!ELEMENT site    (regions, categories, catgraph, people, open_auctions, closed_auctions)>",
    "<!ELEMENT categories      (category+)>",
    "<!ELEMENT category        (name, description)>",
    "<!ATTLIST category        id ID #REQUIRED>",
    "<!ELEMENT name    (#PCDATA)>",
    "<!ELEMENT description     (text | parlist)>",
    "<!ELEMENT text    (#PCDATA | bold | keyword | emph)*>",
    "<!ELEMENT bold		  (#PCDATA | bold | keyword | emph)*>",
    "<!ELEMENT keyword	  (#PCDATA | bold | keyword | emph)*>",
    "<!ELEMENT emph		  (#PCDATA | bold | keyword | emph)*>",
    "<!ELEMENT parlist	  (listitem)*>",
    "<!ELEMENT listitem        (text | parlist)*>",
    "",
    "<!ELEMENT catgraph        (edge*)>",
    "<!ELEMENT edge            EMPTY>",
    "<!ATTLIST edge            from IDREF #REQUIRED to IDREF #REQUIRED>",
    "",
    "<!ELEMENT regions (africa, asia, australia, europe, namerica, samerica)>",
    "<!ELEMENT africa  (item*)>",
    "<!ELEMENT asia    (item*)>",
    "<!ELEMENT australia       (item*)>",
    "<!ELEMENT namerica        (item*)>",
    "<!ELEMENT samerica        (item*)>",
    "<!ELEMENT europe  (item*)>",
    "<!ELEMENT item    (location, quantity, name, payment, description, shipping, incategory+, mailbox)>",
    "<!ATTLIST item            id ID #REQUIRED",
    "                          featured CDATA #IMPLIED>",
    "<!ELEMENT location        (#PCDATA)>",
    "<!ELEMENT quantity        (#PCDATA)>",
    "<!ELEMENT payment (#PCDATA)>",
    "<!ELEMENT shipping        (#PCDATA)>",
    "<!ELEMENT reserve (#PCDATA)>",
    "<!ELEMENT incategory      EMPTY>",
    "<!ATTLIST incategory      category IDREF #REQUIRED>",
    "<!ELEMENT mailbox (mail*)>",
    "<!ELEMENT mail    (from, to, date, text)>",
    "<!ELEMENT from    (#PCDATA)>",
    "<!ELEMENT to      (#PCDATA)>",
    "<!ELEMENT date    (#PCDATA)>",
    "<!ELEMENT itemref         EMPTY>",
    "<!ATTLIST itemref         item IDREF #REQUIRED>",
    "<!ELEMENT personref       EMPTY>",
    "<!ATTLIST personref       person IDREF #REQUIRED>",
    "",
    "<!ELEMENT people  (person*)>",
    "<!ELEMENT person  (name, emailaddress, phone?, address?, homepage?, creditcard?, profile?, watches?)>",
    "<!ATTLIST person          id ID #REQUIRED>",
    "<!ELEMENT emailaddress    (#PCDATA)>",
    "<!ELEMENT phone   (#PCDATA)>",
    "<!ELEMENT address (street, city, country, province?, zipcode)>",
    "<!ELEMENT street  (#PCDATA)>",
    "<!ELEMENT city    (#PCDATA)>",
    "<!ELEMENT province        (#PCDATA)>",
    "<!ELEMENT zipcode (#PCDATA)>",
    "<!ELEMENT country (#PCDATA)>",
    "<!ELEMENT homepage        (#PCDATA)>",
    "<!ELEMENT creditcard      (#PCDATA)>",
    "<!ELEMENT profile (interest*, education?, gender?, business, age?)>",
    "<!ATTLIST profile         income CDATA #IMPLIED>",
    "<!ELEMENT interest        EMPTY>",
    "<!ATTLIST interest        category IDREF #REQUIRED>",
    "<!ELEMENT education       (#PCDATA)>",
    "<!ELEMENT income  (#PCDATA)>",
    "<!ELEMENT gender  (#PCDATA)>",
    "<!ELEMENT business        (#PCDATA)>",
    "<!ELEMENT age     (#PCDATA)>",
    "<!ELEMENT watches (watch*)>",
    "<!ELEMENT watch           EMPTY>",
    "<!ATTLIST watch           open_auction IDREF #REQUIRED>",
    "",
    "<!ELEMENT open_auctions   (open_auction*)>",
    "<!ELEMENT open_auction    (initial, reserve?, bidder*, current, privacy?, itemref, seller, annotation, quantity, type, interval)>",
    "<!ATTLIST open_auction    id ID #REQUIRED>",
    "<!ELEMENT privacy (#PCDATA)>",
    "<!ELEMENT initial (#PCDATA)>",
    "<!ELEMENT bidder  (date, time, personref, increase)>",
    "<!ELEMENT seller          EMPTY>",
    "<!ATTLIST seller          person IDREF #REQUIRED>",
    "<!ELEMENT current (#PCDATA)>",
    "<!ELEMENT increase        (#PCDATA)>",
    "<!ELEMENT type    (#PCDATA)>",
    "<!ELEMENT interval        (start, end)>",
    "<!ELEMENT start   (#PCDATA)>",
    "<!ELEMENT end     (#PCDATA)>",
    "<!ELEMENT time    (#PCDATA)>",
    "<!ELEMENT status  (#PCDATA)>",
    "<!ELEMENT amount  (#PCDATA)>",
    "",
    "<!ELEMENT closed_auctions (closed_auction*)>",
    "<!ELEMENT closed_auction  (seller, buyer, itemref, price, date, quantity, type, annotation?)>",
    "<!ELEMENT buyer           EMPTY>",
    "<!ATTLIST buyer           person IDREF #REQUIRED>",
    "<!ELEMENT price   (#PCDATA)>",
    "<!ELEMENT annotation      (author, description?, happiness)>",
    "",
    "<!ELEMENT author          EMPTY>",
    "<!ATTLIST author          person IDREF #REQUIRED>",
    "<!ELEMENT happiness       (#PCDATA)>",
    NULL
};

pub enum {
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
}

pub typedef {
    int type;
    double mean, dev, min, max;
} ProbDesc;

pub enum {
    ATTR_TYPE_1 = 1,
    ATTR_TYPE_2 = 2,
    ATTR_TYPE_3 = 3
}

pub typedef {
    char name[20];
    int type;
    int ref;
    float prcnt;
} AttDesc;

pub typedef {
    int size, id;
    SetDesc *next;
} SetDesc;

pub typedef {
    int cur, out, brosout;
    int max, brosmax;
    int dir, mydir;
    int current;
} idrepro;



pub typedef {
    int id;

    // element-name, the string inside the angle brackets.
    char *name;

    // ids of children elements.
    int elm[20];
    AttDesc att[5];
    int type;
    SetDesc set;
    int flag;
} Element;

Element objs[]={
    { .id = 0, .name = "*error*" },

    //
    // missing
    //
    // "<!ELEMENT bold		  (#PCDATA | bold | keyword | emph)*>",
    // "<!ELEMENT keyword	  (#PCDATA | bold | keyword | emph)*>",
    // "<!ELEMENT emph		  (#PCDATA | bold | keyword | emph)*>",
    //

    // "<!ELEMENT site (regions, categories, catgraph, people, open_auctions, closed_auctions)>",
    {
        .id = AUCTION_SITE,
        .name = "site",
        .elm = {
            REGION,
            CATEGORY_LIST,
            CATGRAPH,
            PERSON_LIST,
            OPEN_TRANS_LIST,
            CLOSED_TRANS_LIST,
        }
    },

    // "<!ELEMENT categories (category+)>",
    {
        .id = CATEGORY_LIST,
        .name = "categories",
        .elm = { CATEGORY }
    },

    // "<!ELEMENT regions (africa, asia, australia, europe, namerica, samerica)>",
    {
        .id = REGION,
        .name = "regions",
        .elm = {
            AFRICA,
            ASIA,
            AUSTRALIA,
            EUROPE,
            NAMERICA,
            SAMERICA
        }
    },

    // "<!ELEMENT europe  (item*)>",
    {
        .id = EUROPE,
        .name = "europe",
        .elm = {
            ITEM
        }
    },

    // "<!ELEMENT australia       (item*)>",
    {
        .id = AUSTRALIA,
        .name = "australia",
        .elm = {
            ITEM
        }
    },

    // "<!ELEMENT africa  (item*)>",
    {
        .id = AFRICA,
        .name = "africa",
        .elm = {
            ITEM
        }
    },

    // "<!ELEMENT namerica        (item*)>",
    {
        .id = NAMERICA,
        .name = "namerica",
        .elm = {
            ITEM
        }
    },

    // "<!ELEMENT samerica        (item*)>",
    {
        .id = SAMERICA,
        .name = "samerica",
        .elm = {
            ITEM
        }
    },

    // "<!ELEMENT asia    (item*)>",
    {
        .id = ASIA,
        .name = "asia",
        .elm = {
            ITEM
        }
    },

    // "<!ELEMENT catgraph        (edge*)>",
    {
        .id = CATGRAPH,
        .name = "catgraph",
        .elm = { EDGE },
        .type = 0x20
    },

    // "<!ELEMENT edge EMPTY>",
    // "<!ATTLIST edge from IDREF #REQUIRED to IDREF #REQUIRED>",
    {
        .id = EDGE,
        .name = "edge",
        .att = {
            {
                .name = "from",
                .type = ATTR_TYPE_2,
                .ref = CATEGORY,
            },
            {
                .name = "to",
                .type = ATTR_TYPE_2,
                .ref = CATEGORY,
            }
        }
    },

    // "<!ELEMENT category (name, description)>",
    // "<!ATTLIST category        id ID #REQUIRED>",
    {
        .id = CATEGORY,
        .name = "category",
        .elm = {
            CATNAME,
            DESCRIPTION
        },
        .att = {
            {
                .name = "id",
                .type = ATTR_TYPE_1,
            }
        }
    },

    // "<!ELEMENT item (location, quantity, name, payment, description, shipping, incategory+, mailbox)>",
    // "<!ATTLIST item            id ID #REQUIRED",
    {
        .id = ITEM,
        .name = "item",
        .elm = {
            LOCATION,
            QUANTITY,
            ITEMNAME,
            PAYMENT,
            DESCRIPTION,
            SHIPPING,
            INCATEGORY,
            MAILBOX
        },
        .att = {
            {
                .name = "id",
                .type = ATTR_TYPE_1,
                },
            {
                .name = "featured",
                .type = ATTR_TYPE_3,
                .prcnt = 0.1}
        },
        .type = 0x40
    },

    // "<!ELEMENT location        (#PCDATA)>",
    // "<!ELEMENT quantity        (#PCDATA)>",
    // "<!ELEMENT payment (#PCDATA)>",
    // "<!ELEMENT name    (#PCDATA)>",
    { .id = LOCATION, .name = "location" },
    { .id = QUANTITY, .name = "quantity" },
    { .id = PAYMENT, .name = "payment" },
    { .id = NAME, .name = "name" },


    { .id = ITEMNAME, .name = "name" },
    { .id = CATNAME, .name = "name" },

    // "<!ELEMENT description     (text | parlist)>",
    {
        .id = DESCRIPTION,
        .name = "description",
        .elm = {
            TEXT,
            PARLIST
        },
        .type = 0x02
    },

    // "<!ELEMENT parlist	  (listitem)*>",
    {
        .id = PARLIST,
        .name = "parlist",
        .elm = {
            LISTITEM
        }
    },

    // "<!ELEMENT text    (#PCDATA | bold | keyword | emph)*>",
    {
        .id = TEXT,
        .name = "text",
        .type = 0x01
    },

    // "<!ELEMENT listitem        (text | parlist)*>",
    {
        .id = LISTITEM,
        .name = "listitem",
        .elm = {
            TEXT,
            PARLIST
        },
        .type = 0x02
    },

    // "<!ELEMENT shipping        (#PCDATA)>",
    // "<!ELEMENT reserve (#PCDATA)>",
    { .id = SHIPPING, .name = "shipping" },
    { .id = RESERVE, .name = "reserve" },

    // "<!ELEMENT incategory      EMPTY>",
    // "<!ATTLIST incategory      category IDREF #REQUIRED>",
    {
        .id = INCATEGORY,
        .name = "incategory",
        .att = {
            {
                .name = "\1",
                .type = ATTR_TYPE_2,
                .ref = CATEGORY,
            }
        }
    },

    // "<!ELEMENT mailbox (mail*)>",
    {
        .id = MAILBOX,
        .name = "mailbox",
        .elm = {
            MAIL
        }
    },

    // "<!ELEMENT mail (from, to, date, text)>",
    {
        .id = MAIL,
        .name = "mail",
        .elm = {
            FROM,
            TO,
            XDATE,
            TEXT
        }
    },

    // "<!ELEMENT from    (#PCDATA)>",
    // "<!ELEMENT to      (#PCDATA)>",
    // "<!ELEMENT date    (#PCDATA)>",
    { .id = FROM, .name = "from" },
    { .id = TO, .name = "to" },
    { .id = XDATE, .name = "date" },


    // "<!ELEMENT people  (person*)>",
    {
        .id = PERSON_LIST,
        .name = "people",
        .elm = {
            PERSON
        }
    },

    // "<!ELEMENT person  (name, emailaddress, phone?, address?, homepage?, creditcard?, profile?, watches?)>",
    // "<!ATTLIST person          id ID #REQUIRED>",
    {
        .id = PERSON,
        .name = "person",
        .elm = {
            NAME,
            EMAIL,
            PHONE,
            ADDRESS,
            HOMEPAGE,
            CREDITCARD,
            PROFILE,
            WATCHES
        },
        .att = {
            {
                .name = "id",
                .type = ATTR_TYPE_1,
            }
        },
        .type = 0x40
    },

    // "<!ELEMENT emailaddress    (#PCDATA)>",
    // "<!ELEMENT phone   (#PCDATA)>",
    // "<!ELEMENT homepage        (#PCDATA)>",
    // "<!ELEMENT creditcard      (#PCDATA)>",
    { .id = EMAIL, .name = "emailaddress" },
    { .id = PHONE, .name = "phone" },
    { .id = HOMEPAGE, .name = "homepage" },
    { .id = CREDITCARD, .name = "creditcard" },

    // "<!ELEMENT address (street, city, country, province?, zipcode)>",
    {
        .id = ADDRESS,
        .name = "address",
        .elm = {
            STREET,
            CITY,
            COUNTRY,
            PROVINCE,
            ZIPCODE
        }
    },

    // "<!ELEMENT street  (#PCDATA)>",
    // "<!ELEMENT city    (#PCDATA)>",
    // "<!ELEMENT province        (#PCDATA)>",
    // "<!ELEMENT zipcode (#PCDATA)>",
    // "<!ELEMENT country (#PCDATA)>",
    { .id = STREET, .name = "street" },
    { .id = CITY, .name = "city" },
    { .id = PROVINCE, .name = "province" },
    { .id = ZIPCODE, .name = "zipcode" },
    { .id = COUNTRY, .name = "country" },

    // "<!ELEMENT profile (interest*, education?, gender?, business, age?)>",
    // "<!ATTLIST profile income CDATA #IMPLIED>",
    {
        .id = PROFILE,
        .name = "profile",
        .elm = {
            INTEREST,
            EDUCATION,
            GENDER,
            BUSINESS,
            AGE
        },
        .att = {
            {
                .name = "income",
                .type = ATTR_TYPE_3,
                .prcnt = 1
            }
        }
    },

    // "<!ELEMENT education       (#PCDATA)>",
    // "<!ELEMENT income  (#PCDATA)>",
    // "<!ELEMENT gender  (#PCDATA)>",
    // "<!ELEMENT business        (#PCDATA)>",
    // "<!ELEMENT age     (#PCDATA)>",
    { .id = EDUCATION, .name = "education" },
    { .id = INCOME, .name = "income" },
    { .id = GENDER, .name = "gender" },
    { .id = BUSINESS, .name = "business" },
    { .id = AGE, .name = "age" },


    // "<!ELEMENT interest        EMPTY>",
    // "<!ATTLIST interest        category IDREF #REQUIRED>",
    {
        .id = INTEREST,
        .name = "interest",
        .att = {
            {
                .name = "\1",
                .type = ATTR_TYPE_2,
                .ref = CATEGORY,
            }
        }
    },

    // "<!ELEMENT watches (watch*)>",
    {
        .id = WATCHES,
        .name = "watches",
        .elm = {
            WATCH
        }
    },

    // "<!ELEMENT watch           EMPTY>",
    // "<!ATTLIST watch           open_auction IDREF #REQUIRED>",
    {
        .id = WATCH,
        .name = "watch",
        .att = {
            {
                .name = "\1",
                .type = ATTR_TYPE_2,
                .ref = OPEN_TRANS,
            }
        }
    },

    // "<!ELEMENT open_auctions   (open_auction*)>",
    {
        .id = OPEN_TRANS_LIST,
        .name = "open_auctions",
        .elm = {
            OPEN_TRANS
        }
    },

    // "<!ELEMENT open_auction    (initial, reserve?, bidder*, current, privacy?, itemref, seller, annotation, quantity, type, interval)>",
    // "<!ATTLIST open_auction    id ID #REQUIRED>",
    {
        .id = OPEN_TRANS,
        .name = "open_auction",
        .elm = {
            INIT_PRICE,
            RESERVE,
            BIDDER,
            CURRENT,
            PRIVACY,
            ITEMREF,
            SELLER,
            ANNOTATION,
            QUANTITY,
            TYPE,
            INTERVAL
        },
        .att = {
            {
                .name = "id",
                .type = ATTR_TYPE_1,
            }
        },
        .type = 0x04|0x40
    },

    // "<!ELEMENT privacy (#PCDATA)>",
    // "<!ELEMENT amount  (#PCDATA)>",
    // "<!ELEMENT current (#PCDATA)>",
    // "<!ELEMENT increase        (#PCDATA)>",
    // "<!ELEMENT type    (#PCDATA)>",
    { .id = PRIVACY, .name = "privacy" },
    { .id = AMOUNT, .name = "amount" },
    { .id = CURRENT, .name = "current" },
    { .id = INCREASE, .name = "increase" },
    { .id = TYPE, .name = "type" },


    // "<!ELEMENT itemref         EMPTY>",
    // "<!ATTLIST itemref         item IDREF #REQUIRED>",
    {
        .id = ITEMREF,
        .name = "itemref",
        .att = {
            {
                .name = "\1",
                .type = ATTR_TYPE_2,
                .ref = ITEM,
            }
        }
    },

    // "<!ELEMENT bidder (date, time, personref, increase)>",
    {
        .id = BIDDER,
        .name = "bidder",
        .elm = {
            XDATE,
            TIME,
            PERSONREF,
            INCREASE
        }
    },

    // "<!ELEMENT time    (#PCDATA)>",
    // "<!ELEMENT status (#PCDATA)>",
    // "<!ELEMENT initial (#PCDATA)>",
    { .id = TIME, .name = "time" },
    { .id = STATUS, .name = "status" },
    { .id = INIT_PRICE, .name = "initial" },

    // "<!ELEMENT personref       EMPTY>",
    // "<!ATTLIST personref       person IDREF #REQUIRED>",
    {
        .id = PERSONREF,
        .name = "personref",
        .att = {
            {
                .name = "\1",
                .type = ATTR_TYPE_2,
                .ref = PERSON,
            }
        }
    },

    // "<!ELEMENT seller          EMPTY>",
    // "<!ATTLIST seller          person IDREF #REQUIRED>",
    {
        .id = SELLER,
        .name = "seller",
        .att = {
            {
                .name = "\1",
                .type = ATTR_TYPE_2,
                .ref = PERSON,
            }
        }
    },

    // "<!ELEMENT interval        (start, end)>",
    {
        .id = INTERVAL,
        .name = "interval",
        .elm = {
            START,
            END
        }
    },

    // "<!ELEMENT start   (#PCDATA)>",
    // "<!ELEMENT end     (#PCDATA)>",
    { .id = START, .name = "start" },
    { .id = END, .name = "end" },


    // "<!ELEMENT closed_auctions (closed_auction*)>",
    {
        .id = CLOSED_TRANS_LIST,
        .name = "closed_auctions",
        .elm = {
            CLOSED_TRANS
        }
    },

    // "<!ELEMENT closed_auction  (seller, buyer, itemref, price, date, quantity, type, annotation?)>",
    {
        .id = CLOSED_TRANS,
        .name = "closed_auction",
        .elm = {
            SELLER,
            BUYER,
            ITEMREF,
            PRICE,
            XDATE,
            QUANTITY,
            TYPE,
            ANNOTATION
        },
        .type = 0x04|0x40
    },

    // "<!ELEMENT price   (#PCDATA)>",
    { .id = PRICE, .name = "price" },

    // "<!ELEMENT buyer           EMPTY>",
    // "<!ATTLIST buyer           person IDREF #REQUIRED>",
    {
        .id = BUYER,
        .name = "buyer",
        .att = {
            {
                .name = "\1",
                .type = ATTR_TYPE_2,
                .ref = PERSON,
            }
        }
    },

    // "<!ELEMENT annotation      (author, description?, happiness)>",
    {
        .id = ANNOTATION,
        .name = "annotation",
        .elm = {
            AUTHOR,
            DESCRIPTION,
            HAPPINESS
        }
    },

    // "<!ELEMENT happiness       (#PCDATA)>",
    { .id = HAPPINESS, .name = "happiness" },

    // "<!ELEMENT author EMPTY>",
    // "<!ATTLIST author          person IDREF #REQUIRED>",
    {
        .id = AUTHOR,
        .name = "author",
        .att = {
            {
                .name = "\1",
                .type = ATTR_TYPE_2,
                .ref = PERSON
            }
        }
    }
};

idrepro idr[2] = {};

pub void InitializeSchema(float global_scale_factor) {
    int nobj = nelem(objs);

    // Reorder the schema so that a node with id "x"
    // is at the same index "x" in the array.
    Element *newobjs = calloc!(nobj, sizeof(Element));
    for (int i = 0; i < nobj; i++) {
        void *src = &objs[i];
        void *dest = &newobjs[objs[i].id];
        memcpy(dest, src, sizeof(Element));
    }
    memcpy(objs,newobjs,sizeof(Element)*nobj);
    free(newobjs);

    Element *root = GetSchemaNode(1);
    FixSetSize(root, global_scale_factor);
    for (int i = 0; i < nobj; i++) {
        objs[i].flag = 0;
    }
    FixReferenceSets(root);
    for (int i = 0; i < nobj; i++) {
        objs[i].flag = 0;
    }

    int items = -1;
    int open = -1;
    for (int i = 0; i < nobj; i++) {
        if (objs[i].id == ITEM) {
            items = objs[i].set.size;
        }
        if (objs[i].id == OPEN_TRANS) {
            open = objs[i].set.size;
        }
    }

    int closed = items - open;
    InitRepro(&idr[0], open, closed);
    InitRepro(&idr[1], closed, open);
    FixSetByEdge("closed_auctions","closed_auction",closed);

}

int InitRepro_direction=0;
void InitRepro(idrepro *rep, int max, int brosmax) {
    rep->out = 0;
    rep->cur = rep->brosout = 0;
    rep->max = max;
    rep->brosmax = brosmax;
    rep->dir = 0;
    rep->mydir = InitRepro_direction++;
    rep->cur = 0;
}

void FixSetSize(Element *element, float global_scale_factor) {
    if (element->flag++) {
        return;
    }

    int *child_id = element->elm;
    while (*child_id) {
        Element *child_element = GetSchemaNode(*child_id);
        ProbDesc pd = probDescForChild(element, *child_id);
        if (pd.min > 1 && (hasID(child_element) || ((child_element->type & 0x04) != 0))) {
            int size=(int)(GenRandomNum(&pd) + 0.5);
            if ((float)size * global_scale_factor > 1) {
                size = (int)((float)size*global_scale_factor);
            } else {
                size = 1;
            }
            child_element->set.size += size;
            FixDist(&pd, size);
        }
        FixSetSize(child_element, global_scale_factor);
        child_id++;
    }
}

void FixReferenceSets(Element *element) {
    if (element->flag++) {
        return;
    }
    int maxref = 0;
    int *child_id = element->elm;
    while (*child_id) {
        Element *son = GetSchemaNode(*child_id);
        ProbDesc pd = probDescForChild(element, *child_id);
        if (pd.min > 1 && !hasID(son)) {
            double local_factor = 1;
            for (int j=0; j<5; j++) {
                if (son->att[j].name[0]=='\0') break;
                if (maxref < objs[son->att[j].ref].set.size) {
                    maxref = objs[son->att[j].ref].set.size;
                }
            }
            if (!maxref) {
                break;
            }
            local_factor = (double) maxref / pd.max;
            int size=(int)(GenRandomNum(&pd) + 0.5);
            if ((float) size*local_factor > 1) {
                size = (int)((float) size*local_factor);
            } else {
                size = 1;
            }
            son->set.size += size;
            FixDist(&pd, size);
        }
        FixReferenceSets(son);
        child_id++;
    }
}

void FixDist(ProbDesc *pd, double val)
{
    pd->min=pd->max=val;
    pd->type=0;
}

void FixSetByEdge(char *father_name, char *son_name, int size)
{
    int nobj=nelem(objs);
    for (int i=0; i<nobj; i++) {
        if (strcmp(father_name,objs[i].name)) {
            continue;
        }
        Element *element = GetSchemaNode(i);
        int *child_id = element->elm;
        while (*child_id) {
            Element *son = GetSchemaNode(*child_id);
            if (!strcmp(son_name, son->name)) {
                ProbDesc pd = probDescForChild(element, *child_id);
                FixDist(&pd, size);
            }
            child_id++;
        }
    }
}

bool hasID(Element *element) {
    for (size_t i = 0; i < nelem(element->att); i++) {
        if (element->att[i].type == 0) return false;
        if (element->att[i].type == 1) return true;
    }
    return false;
}

pub Element *GetSchemaNode(int id) {
    return objs + id;
}

pub char **getdtd() {
    return dtd;
}

pub idrepro *getidr(int i) {
    return &idr[i];
}


double GenRandomNum(ProbDesc *pd) {
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

typedef {
    int id;
    int child_id;
    ProbDesc p;
} prob_desc_map_item_t;

prob_desc_map_item_t prob_desc_map[] = {
    { CATEGORY_LIST, CATEGORY, {1, 0, 0, 1000, 1000} },
    { EUROPE, ITEM, {1, 0, 0, 6000, 6000} },
    { AUSTRALIA, ITEM, {1, 0, 0, 2200, 2200} },
    { AFRICA, ITEM, {1, 0, 0, 550, 550} },
    { NAMERICA, ITEM, {1, 0, 0, 10000, 10000} },
    { SAMERICA, ITEM, {1, 0, 0, 1000, 1000} },
    { ASIA, ITEM, {1, 0, 0, 2000, 2000} },
    { CATGRAPH, EDGE, {1, 0, 0, 3800, 3800} },
    { ITEM, INCATEGORY, {3,3,0,1,10} },
    { DESCRIPTION, TEXT, {1, 0.7, 0, 0, 0} },
    { DESCRIPTION, PARLIST, {1, 0.3, 0, 0, 0} },
    { PARLIST, LISTITEM, {3,1,0,2,5} },
    { LISTITEM, TEXT, {1,0.8, 0,0,0} },
    { LISTITEM, PARLIST, {1,0.2, 0,0,0} },
    { ADDRESS, PROVINCE, {1,0,0,0,1} },
    { PROFILE, INTEREST,   {3,3,0,0,25} },
    { PROFILE, EDUCATION,  {1,0,0,0,1}  },
    { PROFILE, GENDER,     {1,0,0,0,1}  },
    { PROFILE, AGE,        {1,0,0,0,1}  },
    { WATCHES, WATCH,      {3,4,0,0,100} },
    { OPEN_TRANS_LIST, OPEN_TRANS, {1,0,0,12000, 12000} },
    { OPEN_TRANS, RESERVE,  {1,0,0,0,1} },
    { OPEN_TRANS, BIDDER,   {3,5,0,0,200} },
    { OPEN_TRANS, PRIVACY,  {1,0,0,0,1} },
    { CLOSED_TRANS_LIST, CLOSED_TRANS, {1,0,0,3000,3000}},
    { MAILBOX, MAIL, {3,1,0,0,250}},
    { PERSON_LIST, PERSON, {1,0,0,25500, 25500}},
    {PERSON, PHONE, {1,0,0,0,1}},
    {PERSON, ADDRESS, {1,0,0,0,1}},
    {PERSON, HOMEPAGE, {1,0,0,0,1}},
    {PERSON, CREDITCARD, {1,0,0,0,1}},
    {PERSON, PROFILE, {1,0,0,0,1}},
    {PERSON, WATCHES, {1,0,0,0,1}}
};

pub ProbDesc probDescForChild(Element *e, int child_id) {
    for (size_t i = 0; i < nelem(prob_desc_map); i++) {
        prob_desc_map_item_t *item = &prob_desc_map[i];
        if (item->id == e->id && item->child_id == child_id) {
            return item->p;
        }
    }
    // shouldn't happen
    if (e->id == EDGE || e->id == ITEMREF || e->id == PERSONREF || e->id == SELLER || e->id == BUYER || e->id == AUTHOR) {
        ProbDesc r = {1, 0, 0, 0, 0};
        return r;
    }
    ProbDesc r = {1, 0, 0, 1, 1};
    return r;
}

typedef {
    int element_id;
    char *attr_name;
    ProbDesc p;
} attr_pd_item_t;

attr_pd_item_t attr_desc_map[] = {
    { EDGE, "from",          {1,0,0,0,0} },
    { EDGE, "to",            {1,0,0,0,0} },
    { CATEGORY, "id",        {0,0,0,0,0} },
    { ITEM, "id",            {0,0,0,0,0} },
    { ITEM, "featured",      {0,0,0,0,0} },
    { INCATEGORY, "\1",      {1,0,0,0,0} },
    { PERSON, "id",          {0,0,0,0,0} },
    { PROFILE, "income",     {0,0,0,0,0} },
    { INTEREST, "\1",        {1,0,0,0,0} },
    { WATCH, "\1",           {1,0,0,0,0} },
    { OPEN_TRANS, "id",      {0,0,0,0,0} },
    { ITEMREF, "\1",         {1,0,0,0,0} },
    { PERSONREF, "\1",       {1,0,0,0,0} },
    { SELLER, "\1",          {2, 0.5, 0.1, 0, 0} },
    { BUYER, "\1",           {2, 0.5, 0.1, 0, 0} },
    { AUTHOR, "\1",          {1,0,0,1,1} }
};

pub ProbDesc probDescForAttr(int element_id, char *attr_name) {
    for (size_t i = 0; i < nelem(attr_desc_map); i++) {
        attr_pd_item_t *item = &attr_desc_map[i];
        if (item->element_id == element_id && !strcmp(item->attr_name, attr_name)) {
            return item->p;
        }
    }
    panic("failed to find prob desc for attr '%s'", attr_name);
}
