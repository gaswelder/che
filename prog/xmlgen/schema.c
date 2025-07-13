#import rnd
#import strings
#import dtd.c

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
	PCDATA,
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

typedef {
	int id;
	char *name;
} name_id_t;

name_id_t names[] = {
	{ PCDATA, "#PCDATA"},
	{ ADDRESS, "address" },
	{ AFRICA, "africa" },
	{ ANNOTATION, "annotation" },
	{ ASIA, "asia" },
	{ AUCTION_SITE, "site" },
	{ AUSTRALIA, "australia" },
	{ AUTHOR, "author" },
	{ BIDDER, "bidder" },
	{ BUYER, "buyer" },
	{ CATEGORY_LIST, "categories" },
	{ CATEGORY, "category" },
	{ CATGRAPH, "catgraph" },
	{ CLOSED_TRANS_LIST, "closed_auctions" },
	{ CLOSED_TRANS, "closed_auction" },
	{ DESCRIPTION, "description"},
	{ END, "end" },	
	{ EUROPE, "europe" },
	{ HAPPINESS, "happiness" },
	{ INCATEGORY, "incategory"},
	{ INIT_PRICE, "initial" },
	{ INTEREST, "interest" },
	{ INTERVAL, "interval" },
	{ ITEM, "item" },
	{ ITEMREF, "itemref" },
	{ LISTITEM, "listitem" },
	{ LOCATION, "location" },
	{ MAIL, "mail" },
	{ MAILBOX, "mailbox" },
	{ NAMERICA, "namerica" },
	{ OPEN_TRANS_LIST, "open_auctions" },
	{ OPEN_TRANS, "open_auction" },
	{ PARLIST, "parlist" },
	{ PERSON_LIST, "people" },
	{ PERSON, "person" },
	{ PERSON, "person" },
	{ PERSONREF, "personref" },
	{ PRICE, "price" },
	{ PRIVACY, "privacy" },
	{ PROFILE, "profile" },
	{ REGION, "regions" },
	{ SAMERICA, "samerica" },
	{ SELLER, "seller" },
	{ START, "start" },
	{ STATUS, "status" },
	{ TEXT, "text" },
	{ TEXT, "text" },
	{ TIME, "time" },
	{ WATCH, "watch" },
	{ WATCHES, "watches" },
	{ XDATE, "date" },
    { AGE, "age" },
    { AMOUNT, "amount" },
    { BUSINESS, "business" },
    { CATNAME, "name" },
    { CITY, "city" },
    { COUNTRY, "country" },
    { CREDITCARD, "creditcard" },
    { CURRENT, "current" },
    { EDUCATION, "education" },
    { EMAIL, "emailaddress" },
    { FROM, "from" },
    { GENDER, "gender" },
    { HOMEPAGE, "homepage" },
    { INCOME, "income" },
    { INCREASE, "increase" },
    { ITEMNAME, "name" },
    { NAME, "name" },
	{ EDGE, "edge" },
    { PAYMENT, "payment" },
    { PHONE, "phone" },
    { PROVINCE, "province" },
    { QUANTITY, "quantity" },
    { RESERVE, "reserve" },
    { SHIPPING, "shipping" },
    { STREET, "street" },
    { TO, "to" },
    { TYPE, "type" },
    { XDATE, "date" },
    { ZIPCODE, "zipcode" },
};

int nameid(char *name) {
	for (size_t i = 0; i < nelem(names); i++) {
		if (strcmp(names[i].name, name) == 0) {
			return names[i].id;
		}
	}
	panic("missing name: %s", name);
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

Element objs[77] = {};

idrepro idr[2] = {};

const char *real_dtd = "
	<!ELEMENT site (regions, categories, catgraph, people, open_auctions, closed_auctions)>
	<!ELEMENT categories (category+)>
	<!ELEMENT regions (africa, asia, australia, europe, namerica, samerica)>
	<!ELEMENT europe  (item*)>
	<!ELEMENT australia       (item*)>
	<!ELEMENT africa  (item*)>
	<!ELEMENT namerica        (item*)>
	<!ELEMENT samerica        (item*)>
	<!ELEMENT asia    (item*)>
	<!ELEMENT catgraph        (edge*)>
	<!ELEMENT edge EMPTY>
	<!ATTLIST edge from IDREF #REQUIRED to IDREF #REQUIRED>
	<!ELEMENT category (name, description)>
	<!ATTLIST category        id ID #REQUIRED>
	<!ELEMENT item (location, quantity, name, payment, description, shipping, incategory+, mailbox)>
	<!ATTLIST item            id ID #REQUIRED featured CDATA #IMPLIED>
	<!ELEMENT location        (#PCDATA)>
	<!ELEMENT quantity        (#PCDATA)>
	<!ELEMENT payment (#PCDATA)>
	<!ELEMENT name    (#PCDATA)>
	<!ELEMENT name    (#PCDATA)>
	<!ELEMENT name    (#PCDATA)>
	<!ELEMENT description     (text | parlist)>
	<!ELEMENT parlist	  (listitem*)>
	<!ELEMENT text    (#PCDATA)>
	<!ELEMENT listitem        (text | parlist)*>
	<!ELEMENT shipping        (#PCDATA)>
	<!ELEMENT reserve (#PCDATA)>
	<!ELEMENT incategory      EMPTY>
	<!ATTLIST incategory      category IDREF #REQUIRED>
	<!ELEMENT mailbox (mail*)>
	<!ELEMENT mail (from, to, date, text)>
	<!ELEMENT from    (#PCDATA)>
	<!ELEMENT to      (#PCDATA)>
	<!ELEMENT date    (#PCDATA)>
	<!ELEMENT people  (person*)>
	<!ELEMENT person  (name, emailaddress, phone?, address?, homepage?, creditcard?, profile?, watches?)>
	<!ATTLIST person          id ID #REQUIRED>
	<!ELEMENT emailaddress    (#PCDATA)>
	<!ELEMENT phone   (#PCDATA)>
	<!ELEMENT homepage        (#PCDATA)>
	<!ELEMENT creditcard      (#PCDATA)>
	<!ELEMENT address (street, city, country, province?, zipcode)>
	<!ELEMENT street  (#PCDATA)>
	<!ELEMENT city    (#PCDATA)>
	<!ELEMENT province        (#PCDATA)>
	<!ELEMENT zipcode (#PCDATA)>
	<!ELEMENT country (#PCDATA)>
	<!ELEMENT profile (interest*, education?, gender?, business, age?)>
	<!ATTLIST profile income CDATA #IMPLIED>
	<!ELEMENT education       (#PCDATA)>
	<!ELEMENT income  (#PCDATA)>
	<!ELEMENT gender  (#PCDATA)>
	<!ELEMENT business        (#PCDATA)>
	<!ELEMENT age     (#PCDATA)>
	<!ELEMENT interest        EMPTY>
	<!ATTLIST interest        category IDREF #REQUIRED>
	<!ELEMENT watches (watch*)>
	<!ELEMENT watch           EMPTY>
	<!ATTLIST watch           open_auction IDREF #REQUIRED>
	<!ELEMENT open_auctions   (open_auction*)>
	<!ELEMENT open_auction    (initial, reserve?, bidder*, current, privacy?, itemref, seller, annotation, quantity,type, interval)>
	<!ATTLIST open_auction    id ID #REQUIRED>
	<!ELEMENT privacy (#PCDATA)>
	<!ELEMENT amount  (#PCDATA)>
	<!ELEMENT current (#PCDATA)>
	<!ELEMENT increase        (#PCDATA)>
	<!ELEMENT type    (#PCDATA)>
	<!ELEMENT itemref         EMPTY>
	<!ATTLIST itemref         item IDREF #REQUIRED>
	<!ELEMENT bidder (date, time, personref, increase)>
	<!ELEMENT time    (#PCDATA)>
	<!ELEMENT status (#PCDATA)>
	<!ELEMENT initial (#PCDATA)>
	<!ELEMENT personref       EMPTY>
	<!ATTLIST personref       person IDREF #REQUIRED>
	<!ELEMENT seller          EMPTY>
	<!ATTLIST seller          person IDREF #REQUIRED>
	<!ELEMENT interval        (start, end)>
	<!ELEMENT start   (#PCDATA)>
	<!ELEMENT end     (#PCDATA)>
	<!ELEMENT closed_auctions (closed_auction*)>
	<!ELEMENT closed_auction  (seller, buyer, itemref, price, date, quantity, type, annotation?)>
	<!ELEMENT price   (#PCDATA)>
	<!ELEMENT buyer           EMPTY>
	<!ATTLIST buyer           person IDREF #REQUIRED>
	<!ELEMENT annotation      (author, description?, happiness)>
	<!ELEMENT happiness       (#PCDATA)>
	<!ELEMENT author EMPTY>
	<!ATTLIST author          person IDREF #REQUIRED>";

const char *loadline(const char *p, char *buf) {
	while (isspace(*p)) p++;
	char *b = buf;
	while (*p != '\0' && *p != '\n') {
		*b++ = *p++;
	}
	*b = '\0';
	return p;
}

pub void InitializeSchema(float global_scale_factor) {
    // Parse the DTD into the local schema.
	char buf[200] = {};
	const char *p = real_dtd;
	while (true) {
		p = loadline(p, buf);
		if (buf[0] == '\0') {
			break;
		}
		if (strings.starts_with(buf, "<!ATTLIST ")) {
			read_attlist(buf);
		} else {
			read_element(buf);
		}
	}

	puts("parsed dtd");

	// Some tweaks to the schema.
	objs[nameid("catgraph")].type = 0x20;
	objs[nameid("item")].type = 0x40;
	objs[nameid("item")].att[1].prcnt = 0.1;
	objs[nameid("description")].type = 0x02;
	objs[nameid("text")].type = 0x01;
	objs[nameid("listitem")].type = 0x02;
	objs[nameid("person")].type = 0x40;
	objs[nameid("profile")].att[0].prcnt = 1;
	objs[nameid("open_auction")].type = 0x04|0x40;
	objs[nameid("closed_auction")].type = 0x04|0x40;


	int nobj = nelem(objs);

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
	
	
	// "fix set by edge"
	Element *closed_auctions = &objs[nameid("closed_auctions")];
	int *child_id = closed_auctions->elm;
	while (*child_id) {
		Element *son = GetSchemaNode(*child_id);
		if (!strcmp("closed_auction", son->name)) {
			ProbDesc pd = probDescForChild(closed_auctions, *child_id);
			FixDist(&pd, closed);
		}
		child_id++;
	}
}




void read_attlist(char *s) {
	dtd.attlist_t attlist = {};
	dtd.read_attlist(s, &attlist);

	int pos = nameid(attlist.host);
	Element *obj = &objs[pos];

	for (int i = 0; i < attlist.size; i++) {
		dtd.att_t *att = &attlist.items[i];
		AttDesc *objatt = &obj->att[i];
		if (att->type == dtd.IDREF && att->flag == dtd.REQUIRED) {
			objatt->name[0] = '\1';
			objatt->type = ATTR_TYPE_2;
			objatt->ref = nameid(att->name);
		} else if (att->type == dtd.ID && att->flag == dtd.REQUIRED) {
			strcpy(objatt->name, att->name);
			objatt->type = ATTR_TYPE_1;
		} else if (att->type == dtd.CDATA && att->flag == dtd.IMPLIED) {
			strcpy(objatt->name, att->name);
			objatt->type = ATTR_TYPE_3;
		} else {
			panic("unhandled combination");
		}
	}	
}

void read_element(char *s) {
	dtd.element_t element = {};
	dtd.read_element(&s, &element);
	dtd.print_element(&element);


	int pos = nameid(element.name);
	objs[pos].id = pos;
	objs[pos].name = strings.newstr("%s", element.name);
	dtd.child_list_t *l = element.children;
	for (int i = 0; i < l->size; i++) {
		if (l->items[i].islist) {
			panic("nested lists not implemented");
		} else {
			dtd.child_t *e = l->items[i].data;
			objs[pos].elm[i] = nameid(e->name);
		}
	}
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
