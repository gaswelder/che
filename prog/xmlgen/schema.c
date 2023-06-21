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
};

pub typedef {
    int type;
    double mean, dev, min, max;
} ProbDesc;

pub typedef {
    int id;
    ProbDesc pd;
    char rec;
} ElmDesc;

pub typedef {
    char name[20];
    int type;
    int ref;
    ProbDesc pd;
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
    
    ElmDesc elm[20];
    AttDesc att[5];
    int type;
    int kids;
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
            {REGION,{1,0,0,1,1}, 0},
            {CATEGORY_LIST,{1,0,0,1,1}, 0},
            {CATGRAPH,{1,0,0,1,1}, 0},
            {PERSON_LIST,{1,0,0,1,1}, 0},
            {OPEN_TRANS_LIST,{1,0,0,1,1}, 0},
            {CLOSED_TRANS_LIST,{1,0,0,1,1}, 0}
        }
    },

    // "<!ELEMENT categories (category+)>",    
    {
        .id = CATEGORY_LIST,
        .name = "categories",
        .elm = {
            {CATEGORY,{1,0,0,1000,1000}, 0}
        }
    },

    // "<!ELEMENT regions (africa, asia, australia, europe, namerica, samerica)>",
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

    // "<!ELEMENT europe  (item*)>",
    {
        .id = EUROPE,
        .name = "europe",
        .elm = {
            {ITEM,{1,0,0,6000,6000}, 0}
        }
    },

    // "<!ELEMENT australia       (item*)>",
    {
        .id = AUSTRALIA,
        .name = "australia",
        .elm = {
            {ITEM,{1,0,0,2200,2200}, 0}
        }
    },

    // "<!ELEMENT africa  (item*)>",
    {
        .id = AFRICA,
        .name = "africa",
        .elm = {
            {ITEM,{1,0,0,550,550}, 0}
        }
    },

    // "<!ELEMENT namerica        (item*)>",
    {
        .id = NAMERICA,
        .name = "namerica",
        .elm = {
            {ITEM,{1,0,0,10000,10000}, 0}
        }
    },

    // "<!ELEMENT samerica        (item*)>",
    {
        .id = SAMERICA,
        .name = "samerica",
        .elm = {
            {ITEM,{1,0,0,1000,1000}, 0}
        }
    },

    // "<!ELEMENT asia    (item*)>",
    {
        .id = ASIA,
        .name = "asia",
        .elm = {
            {ITEM,{1,0,0,2000,2000}, 0}
        }
    },

    // "<!ELEMENT catgraph        (edge*)>",
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

    // "<!ELEMENT edge EMPTY>",
    // "<!ATTLIST edge from IDREF #REQUIRED to IDREF #REQUIRED>",
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

    // "<!ELEMENT category (name, description)>",
    // "<!ATTLIST category        id ID #REQUIRED>",
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

    // "<!ELEMENT item (location, quantity, name, payment, description, shipping, incategory+, mailbox)>",
    // "<!ATTLIST item            id ID #REQUIRED",
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

    // "<!ELEMENT parlist	  (listitem)*>",
    {
        .id = PARLIST,
        .name = "parlist",
        .elm = {
            {LISTITEM,{3,1,0,2,5}, 0}
        }
    },

    // "<!ELEMENT text    (#PCDATA | bold | keyword | emph)*>",
    {
        .id = TEXT,
        .name = "text",
        .elm = {
            {0,{0,0,0,0,0}, 0}
        },
        .att = {{.name = "",0,0,{0,0,0,0,0}}},
        0x01
    },

    // "<!ELEMENT listitem        (text | parlist)*>",
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

    // "<!ELEMENT shipping        (#PCDATA)>",
    // "<!ELEMENT reserve (#PCDATA)>",
    { .id = SHIPPING, .name = "shipping" },
    { .id = RESERVE, .name = "reserve" },

    // "<!ELEMENT incategory      EMPTY>",
    // "<!ATTLIST incategory      category IDREF #REQUIRED>",
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

    // "<!ELEMENT mailbox (mail*)>",
    {
        .id = MAILBOX,
        .name = "mailbox",
        .elm = {
            {MAIL,{3,1,0,0,250}, 0}
        }
    },

    // "<!ELEMENT mail (from, to, date, text)>",
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
            {PERSON,{1,0,0,25500,25500}, 0}
        }
    },

    // "<!ELEMENT person  (name, emailaddress, phone?, address?, homepage?, creditcard?, profile?, watches?)>",
    // "<!ATTLIST person          id ID #REQUIRED>",
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

    // "<!ELEMENT emailaddress    (#PCDATA)>",
    { .id = EMAIL, .name = "emailaddress" },

    // "<!ELEMENT phone   (#PCDATA)>",
    // "<!ELEMENT homepage        (#PCDATA)>",
    // "<!ELEMENT creditcard      (#PCDATA)>",
    { .id = PHONE, .name = "phone" },
    { .id = HOMEPAGE, .name = "homepage" },
    { .id = CREDITCARD, .name = "creditcard" },

    // "<!ELEMENT address (street, city, country, province?, zipcode)>",
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

    // "<!ELEMENT street  (#PCDATA)>",
    { .id = STREET, .name = "street" },

    // "<!ELEMENT city    (#PCDATA)>",
    { .id = CITY, .name = "city" },

    // "<!ELEMENT province        (#PCDATA)>",
    { .id = PROVINCE, .name = "province" },

    // "<!ELEMENT zipcode (#PCDATA)>",
    { .id = ZIPCODE, .name = "zipcode" },

    // "<!ELEMENT country (#PCDATA)>",
    { .id = COUNTRY, .name = "country" },

    // "<!ELEMENT profile (interest*, education?, gender?, business, age?)>",
    // "<!ATTLIST profile income CDATA #IMPLIED>",
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
        .elm = {{0,{0,0,0,0,0}, 0}},
        .att = {
            {
                .name = "\1",
                .type = 2,CATEGORY,{1,0,0,0,0}}
        }
    },

    // "<!ELEMENT watches (watch*)>",
    {
        .id = WATCHES,
        .name = "watches",
        .elm = {
            {WATCH,{3,4,0,0,100}, 0},
        }
    },

    // "<!ELEMENT watch           EMPTY>",
    // "<!ATTLIST watch           open_auction IDREF #REQUIRED>",
    {
        .id = WATCH,
        .name = "watch",
        .elm = {{0,{0,0,0,0,0}, 0}},
        .att = {
            {.name = "\1", .type = 2,OPEN_TRANS,{1,0,0,0,0}}
        }
    },

    // "<!ELEMENT open_auctions   (open_auction*)>",
    {
        .id = OPEN_TRANS_LIST,
        .name = "open_auctions",
        .elm = {
            {OPEN_TRANS,{1,0,0,12000,12000}, 0}
        }
    },

    // "<!ELEMENT open_auction    (initial, reserve?, bidder*, current, privacy?, itemref, seller, annotation, quantity, type, interval)>",
    // "<!ATTLIST open_auction    id ID #REQUIRED>",
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
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {.name = "\1", .type = 2,ITEM,{1,0,0,0,0}}
        }
    },

    // "<!ELEMENT bidder (date, time, personref, increase)>",
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
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {
                .name = "\1",
                .type = 2,
                PERSON,{1,0,0,0,0}
            }
        }
    },

    // "<!ELEMENT seller          EMPTY>",
    // "<!ATTLIST seller          person IDREF #REQUIRED>",
    {
        .id = SELLER,
        .name = "seller",
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {.name = "\1",
            .type = 2,PERSON,{2,0.5,0.10,0,0}}
        }
    },

    // "<!ELEMENT interval        (start, end)>",
    {
        .id = INTERVAL,
        .name = "interval",
        .elm = {
            {START,{1,0,0,1,1}, .rec = 0},
            {END,{1,0,0,1,1}, .rec = 0}
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
            {CLOSED_TRANS,{1,0,0,3000,3000}, .rec = 0}
        }
    },

    // "<!ELEMENT closed_auction  (seller, buyer, itemref, price, date, quantity, type, annotation?)>",
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

    // "<!ELEMENT price   (#PCDATA)>",
    { .id = PRICE, .name = "price" },

    // "<!ELEMENT buyer           EMPTY>",
    // "<!ATTLIST buyer           person IDREF #REQUIRED>",
    {
        .id = BUYER,
        .name = "buyer",
        .elm = {{0,{0,0,0,0,0}, .rec = 0}},
        .att = {
            {.name = "\1",.type = 2,PERSON,{2,0.5,0.10,0,0}}
        }
    },

    // "<!ELEMENT annotation      (author, description?, happiness)>",
    {
        .id = ANNOTATION,
        .name = "annotation",
        .elm = {
            {AUTHOR,{1,0,0,1,1}, .rec = 0},
            {DESCRIPTION,{1,0,0,1,1}, .rec = 0},
            {HAPPINESS,{1,0,0,1,1}, .rec = 0}
        }
    },

    // "<!ELEMENT happiness       (#PCDATA)>",
    { .id = HAPPINESS, .name = "happiness" },

    // "<!ELEMENT author EMPTY>",
    // "<!ATTLIST author          person IDREF #REQUIRED>",
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

idrepro idr[2] = {};

pub void InitializeSchema(float global_scale_factor) {
    int nobj = nelem(objs);
    
    // Reorder the schema so that a node with id "x"
    // is at the same index "x" in the array.
    Element *newobjs = calloc(nobj, sizeof(Element));
    for (int i = 0; i < nobj; i++) {
        void *src = &objs[i];
        void *dest = &newobjs[objs[i].id];
        memcpy(dest, src, sizeof(Element));
    }
    memcpy(objs,newobjs,sizeof(Element)*nobj);
    free(newobjs);

    // Initialize the "kids" field on each node:
    // the count of items it has in its "elm" field.
    for (int i = 0; i < nobj; i++) {
        for (int j=0; j<20; j++) {
            if (objs[i].elm[j].id != 0) {
                objs[i].kids++;
            }
        }
    }

    Element *root = GetSchemaNode(1);
    FixSetSize(root, global_scale_factor);
    for (int i = 0; i < nobj; i++) {
        objs[i].flag = 0;
    }
    FixReferenceSets(root);
    for (int i = 0; i < nobj; i++) {
        objs[i].flag = 0;
    }
    CheckRecursion();
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

void FixSetSize(Element *od, float global_scale_factor) {
    if (od->flag++) {
        return;
    }
    for (int i = 0; i < od->kids; i++) {
        ElmDesc *ed = &(od->elm[i]);
        Element *son = GetSchemaNode(ed->id);
        if (!son) continue;
        if (ed->pd.min>1 && (hasID(son) || (son->type&0x04))) {
            int size=(int)(GenRandomNum(&ed->pd)+0.5);
            if (size*global_scale_factor > 1) {
                size = (int)(size*global_scale_factor);
            } else {
                size = 1;
            }
            son->set.size+=size;
            FixDist(&ed->pd,size);
        }
        FixSetSize(son, global_scale_factor);
    }
}

void FixReferenceSets(Element *od)
{
    int i,j,maxref=0;
    if (od->flag++) return;
    for (i=0;i<od->kids;i++)
        {
            ElmDesc *ed=&(od->elm[i]);
            Element *son = GetSchemaNode(ed->id);
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
        Element *od=objs+i;
        for (int j=0;j<od->kids;j++) {
            ElmDesc *ed=&(od->elm[j]);
            Element *son=objs+ed->id;
            if (!strcmp(son_name,son->name)) {
                FixDist(&ed->pd,size);
            }
        }
    }
}

void CheckRecursion() {
    int n = nelem(objs);
    for (int i = 1; i < n; i++) {
        Element *root = &objs[i];
        if (root->type != 0x02) {
            continue;
        }
        for (int j=0; j < root->kids; j++) {
            root->elm[j].rec = FindRec(&objs[root->elm[j].id], root);
        }
    }
}

int FindRec(Element *od, Element *search) {
    if (od == search) {
        od->flag=0;
        return 1;
    }

    int r = 0;
    if (!od->flag) {
        od->flag = 1;
        for (int i=0; i < od->kids; i++) {
            r += FindRec(objs + od->elm[i].id, search);
        }
    }
    od->flag=0;
    return r;
}

bool hasID(Element *od) {
    for (size_t i = 0; i < nelem(od->att); i++) {
        if (od->att[i].type == 0) return false;
        if (od->att[i].type == 1) return true;
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
