#import rnd
#import strings
#import formats/dtd

enum {
	PCDATA = 1,
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
	{ ADDRESS, "address" },
	{ AFRICA, "africa" },
	{ AGE, "age" },
	{ AMOUNT, "amount" },
	{ ANNOTATION, "annotation" },
	{ ASIA, "asia" },
	{ AUCTION_SITE, "site" },
	{ AUSTRALIA, "australia" },
	{ AUTHOR, "author" },
	{ BIDDER, "bidder" },
	{ BUSINESS, "business" },
	{ BUYER, "buyer" },
	{ CATEGORY_LIST, "categories" },
	{ CATEGORY, "category" },
	{ CATGRAPH, "catgraph" },
	{ CATNAME, "name" },
	{ CITY, "city" },
	{ CLOSED_TRANS_LIST, "closed_auctions" },
	{ CLOSED_TRANS, "closed_auction" },
	{ COUNTRY, "country" },
	{ CREDITCARD, "creditcard" },
	{ CURRENT, "current" },
	{ DESCRIPTION, "description"},
	{ EDGE, "edge" },
	{ EDUCATION, "education" },
	{ EMAIL, "emailaddress" },
	{ END, "end" },	
	{ EUROPE, "europe" },
	{ FROM, "from" },
	{ GENDER, "gender" },
	{ HAPPINESS, "happiness" },
	{ HOMEPAGE, "homepage" },
	{ INCATEGORY, "incategory"},
	{ INCOME, "income" },
	{ INCREASE, "increase" },
	{ INIT_PRICE, "initial" },
	{ INTEREST, "interest" },
	{ INTERVAL, "interval" },
	{ ITEM, "item" },
	{ ITEMNAME, "name" },
	{ ITEMREF, "itemref" },
	{ LISTITEM, "listitem" },
	{ LOCATION, "location" },
	{ MAIL, "mail" },
	{ MAILBOX, "mailbox" },
	{ NAME, "name" },
	{ NAMERICA, "namerica" },
	{ OPEN_TRANS_LIST, "open_auctions" },
	{ OPEN_TRANS, "open_auction" },
	{ PARLIST, "parlist" },
	{ PAYMENT, "payment" },
	{ PCDATA, "#PCDATA"},
	{ PERSON_LIST, "people" },
	{ PERSON, "person" },
	{ PERSONREF, "personref" },
	{ PHONE, "phone" },
	{ PRICE, "price" },
	{ PRIVACY, "privacy" },
	{ PROFILE, "profile" },
	{ PROVINCE, "province" },
	{ QUANTITY, "quantity" },
	{ REGION, "regions" },
	{ RESERVE, "reserve" },
	{ SAMERICA, "samerica" },
	{ SELLER, "seller" },
	{ SHIPPING, "shipping" },
	{ START, "start" },
	{ STATUS, "status" },
	{ STREET, "street" },
	{ TEXT, "text" },
	{ TIME, "time" },
	{ TO, "to" },
	{ TYPE, "type" },
	{ WATCH, "watch" },
	{ WATCHES, "watches" },
	{ XDATE, "date" },
	{ ZIPCODE, "zipcode" },
};

pub int nameid(char *name) {
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
	<!ELEMENT australia	   (item*)>
	<!ELEMENT africa  (item*)>
	<!ELEMENT namerica		(item*)>
	<!ELEMENT samerica		(item*)>
	<!ELEMENT asia	(item*)>
	<!ELEMENT catgraph		(edge*)>
	<!ELEMENT edge EMPTY>
	<!ATTLIST edge from IDREF #REQUIRED to IDREF #REQUIRED>
	<!ELEMENT category (name, description)>
	<!ATTLIST category		id ID #REQUIRED>
	<!ELEMENT item (location, quantity, name, payment, description, shipping, incategory+, mailbox)>
	<!ATTLIST item			id ID #REQUIRED featured CDATA #IMPLIED>
	<!ELEMENT location		(#PCDATA)>
	<!ELEMENT quantity		(#PCDATA)>
	<!ELEMENT payment (#PCDATA)>
	<!ELEMENT name	(#PCDATA)>
	<!ELEMENT name	(#PCDATA)>
	<!ELEMENT name	(#PCDATA)>
	<!ELEMENT description	 (text | parlist)>
	<!ELEMENT parlist	  (listitem*)>
	<!ELEMENT text	(#PCDATA)>
	<!ELEMENT listitem		(text | parlist)*>
	<!ELEMENT shipping		(#PCDATA)>
	<!ELEMENT reserve (#PCDATA)>
	<!ELEMENT incategory	  EMPTY>
	<!ATTLIST incategory	  category IDREF #REQUIRED>
	<!ELEMENT mailbox (mail*)>
	<!ELEMENT mail (from, to, date, text)>
	<!ELEMENT from	(#PCDATA)>
	<!ELEMENT to	  (#PCDATA)>
	<!ELEMENT date	(#PCDATA)>
	<!ELEMENT people  (person*)>
	<!ELEMENT person  (name, emailaddress, phone?, address?, homepage?, creditcard?, profile?, watches?)>
	<!ATTLIST person		  id ID #REQUIRED>
	<!ELEMENT emailaddress	(#PCDATA)>
	<!ELEMENT phone   (#PCDATA)>
	<!ELEMENT homepage		(#PCDATA)>
	<!ELEMENT creditcard	  (#PCDATA)>
	<!ELEMENT address (street, city, country, province?, zipcode)>
	<!ELEMENT street  (#PCDATA)>
	<!ELEMENT city	(#PCDATA)>
	<!ELEMENT province		(#PCDATA)>
	<!ELEMENT zipcode (#PCDATA)>
	<!ELEMENT country (#PCDATA)>
	<!ELEMENT profile (interest*, education?, gender?, business, age?)>
	<!ATTLIST profile income CDATA #IMPLIED>
	<!ELEMENT education	   (#PCDATA)>
	<!ELEMENT income  (#PCDATA)>
	<!ELEMENT gender  (#PCDATA)>
	<!ELEMENT business		(#PCDATA)>
	<!ELEMENT age	 (#PCDATA)>
	<!ELEMENT interest		EMPTY>
	<!ATTLIST interest		category IDREF #REQUIRED>
	<!ELEMENT watches (watch*)>
	<!ELEMENT watch		   EMPTY>
	<!ATTLIST watch		   open_auction IDREF #REQUIRED>
	<!ELEMENT open_auctions   (open_auction*)>
	<!ELEMENT open_auction	(initial, reserve?, bidder*, current, privacy?, itemref, seller, annotation, quantity,type, interval)>
	<!ATTLIST open_auction	id ID #REQUIRED>
	<!ELEMENT privacy (#PCDATA)>
	<!ELEMENT amount  (#PCDATA)>
	<!ELEMENT current (#PCDATA)>
	<!ELEMENT increase		(#PCDATA)>
	<!ELEMENT type	(#PCDATA)>
	<!ELEMENT itemref		 EMPTY>
	<!ATTLIST itemref		 item IDREF #REQUIRED>
	<!ELEMENT bidder (date, time, personref, increase)>
	<!ELEMENT time	(#PCDATA)>
	<!ELEMENT status (#PCDATA)>
	<!ELEMENT initial (#PCDATA)>
	<!ELEMENT personref	   EMPTY>
	<!ATTLIST personref	   person IDREF #REQUIRED>
	<!ELEMENT seller		  EMPTY>
	<!ATTLIST seller		  person IDREF #REQUIRED>
	<!ELEMENT interval		(start, end)>
	<!ELEMENT start   (#PCDATA)>
	<!ELEMENT end	 (#PCDATA)>
	<!ELEMENT closed_auctions (closed_auction*)>
	<!ELEMENT closed_auction  (seller, buyer, itemref, price, date, quantity, type, annotation?)>
	<!ELEMENT price   (#PCDATA)>
	<!ELEMENT buyer		   EMPTY>
	<!ATTLIST buyer		   person IDREF #REQUIRED>
	<!ELEMENT annotation	  (author, description?, happiness)>
	<!ELEMENT happiness	   (#PCDATA)>
	<!ELEMENT author EMPTY>
	<!ATTLIST author		  person IDREF #REQUIRED>";

pub void InitializeSchema(float global_scale_factor) {
	dtd.schema_t schema = dtd.parse(real_dtd);
	for (int i = 0; i < schema.elemnum; i++) {
		import_element(&schema.elements[i]);
	}
	for (int i = 0; i < schema.attrnum; i++) {
		import_attlist(&schema.attlists[i]);
	}

	puts("parsed dtd");

	// Some tweaks to the schema.
	objs[nameid("item")].att[1].prcnt = 0.1;
	objs[nameid("profile")].att[0].prcnt = 1;


	int nobj = nelem(objs);

	for (int i = 0; i < nobj; i++) {
		printf("%d\t%s (%d)\n", i, objs[i].name, objs[i].id);
	}

	Element *root = GetSchemaNode(1);
	FixSetSize(root, global_scale_factor);

	//
	// Reset flag in all objects.
	//
	for (int i = 0; i < nobj; i++) {
		objs[i].flag = 0;
	}


	FixReferenceSets(root);

	//
	// Reset flag in all objects.
	//
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
}

void import_attlist(dtd.attlist_t *attlist) {
	printf("\timport attlist %s\n", attlist->host);
	int pos = nameid(attlist->host);
	Element *obj = &objs[pos];

	for (int i = 0; i < attlist->size; i++) {
		dtd.att_t *att = &attlist->items[i];
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

void import_element(dtd.element_t *element) {
	printf("import %s\n", element->name);
	int pos = nameid(element->name);
	objs[pos].id = pos;
	objs[pos].name = strings.newstr("%s", element->name);
	dtd.child_list_t *l = element->children;
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
	//
	// If flag was set, skip
	//
	if (element->flag++) {
		return;
	}

	//
	// Loop through children
	//
	int *child_id = element->elm;
	while (*child_id) {
		Element *child_element = GetSchemaNode(*child_id);
		ProbDesc pd = probDescForChild(element, *child_id);

		//
		//
		//
		bool cond1 = pd.min > 1 && (
			hasID(child_element)
			|| *child_id == nameid("open_auction")
			|| *child_id == nameid("closed_auction")
		);

		if (cond1) {
			//
			//
			//
			double size = 0.5 + GenRandomNum(&pd);
			size *= global_scale_factor;
			if (size < 1) {
				size = 1;
			}

			child_element->set.size += (int) size;
		}

		//
		//
		//
		FixSetSize(child_element, global_scale_factor);
		child_id++;
	}
}

void FixReferenceSets(Element *element) {
	//
	// If flag was set, skip
	//
	if (element->flag++) {
		return;
	}
	int maxref = 0;

	//
	// Loop over children
	//
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
			pd.min = size;
			pd.max = size;
			pd.type = 0;
		}
		FixReferenceSets(son);
		child_id++;
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

pub idrepro *getidr(int i) {
	return &idr[i];
}

enum {
	UNIF = 1,
	GAUSS = 2,
	EXP = 3,
}

double clamp(double x, min, max) {
	if (x < min) x = min;
	if (x > max) x = max;
	return x;
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

	switch (pd->type) {
		case UNIF: {
			return rnd.urange(pd->min, pd->max);
		}
		case GAUSS: {
			double res = pd->mean + pd->dev * rnd.gauss();
			return clamp(res, pd->min, pd->max);
		}
		case EXP: {
			double res = pd->min + rnd.exponential(pd->mean);
			return clamp(res, pd->min, pd->max);
		}
	}
	panic("undefined distribution: %d", pd->type);
}

typedef {
	int id;
	int child_id;
	ProbDesc p;
} prob_desc_map_item_t;

typedef {
	int element_id;
	char *attr_name;
	ProbDesc p;
} attr_pd_item_t;


// { elem, child, { type, mean, dev, min, max} }
prob_desc_map_item_t prob_desc_map[] = {
	{ CATEGORY_LIST, CATEGORY,			{ UNIF, 0, 0, 1000, 1000} },
	{ EUROPE, ITEM, 					{ UNIF, 0, 0, 6000, 6000} },
	{ AUSTRALIA, ITEM,					{ UNIF, 0, 0, 2200, 2200} },
	{ AFRICA, ITEM,						{ UNIF, 0, 0, 550, 550} },
	{ NAMERICA, ITEM,					{ UNIF, 0, 0, 10000, 10000} },
	{ SAMERICA, ITEM,					{ UNIF, 0, 0, 1000, 1000} },
	{ ASIA, ITEM,						{ UNIF, 0, 0, 2000, 2000} },
	{ CATGRAPH, EDGE,					{ UNIF, 0, 0, 3800, 3800} },
	{ ITEM, INCATEGORY,					{ EXP,  3, 0,1,10} },
	{ DESCRIPTION, TEXT,				{ UNIF, 0.7, 0, 0, 0} },
	{ DESCRIPTION, PARLIST,				{ UNIF, 0.3, 0, 0, 0} },
	{ PARLIST, LISTITEM,				{ EXP,  1,0,2,5} },
	{ LISTITEM, TEXT,					{ UNIF, 0.8, 0,0,0} },
	{ LISTITEM, PARLIST,				{ UNIF, 0.2, 0,0,0} },
	{ ADDRESS, PROVINCE,				{ UNIF, 0,0,0,1} },
	{ PROFILE, INTEREST,  				{ EXP,  3,0,0,25} },
	{ PROFILE, EDUCATION, 				{ UNIF, 0,0,0,1}  },
	{ PROFILE, GENDER,					{ UNIF, 0,0,0,1}  },
	{ PROFILE, AGE,						{ UNIF, 0,0,0,1}  },
	{ WATCHES, WATCH,	 				{ EXP,  4,0,0,100} },
	{ OPEN_TRANS_LIST, OPEN_TRANS,		{ UNIF, 0,0,12000, 12000} },
	{ OPEN_TRANS, RESERVE, 				{ UNIF, 0,0,0,1} },
	{ OPEN_TRANS, BIDDER,  				{ EXP,  5,0,0,200} },
	{ OPEN_TRANS, PRIVACY, 				{ UNIF, 0,0,0,1} },
	{ CLOSED_TRANS_LIST, CLOSED_TRANS,	{ UNIF, 0,0,3000,3000}},
	{ MAILBOX, MAIL,					{ EXP,  1,0,0,250}},
	{ PERSON_LIST, PERSON,				{ UNIF, 0,0,25500, 25500}},
	{PERSON, PHONE,						{ UNIF, 0,0,0,1}},
	{PERSON, ADDRESS,					{ UNIF, 0,0,0,1}},
	{PERSON, HOMEPAGE,					{ UNIF, 0,0,0,1}},
	{PERSON, CREDITCARD,				{ UNIF, 0,0,0,1}},
	{PERSON, PROFILE,					{ UNIF, 0,0,0,1}},
	{PERSON, WATCHES, 					{ UNIF, 0,0,0,1}}
};

attr_pd_item_t attr_desc_map[] = {
	{ EDGE, "from",			{ UNIF,0,0,0,0} },
	{ EDGE, "to",			{ UNIF,0,0,0,0} },
	{ CATEGORY, "id",		{ 0,0,0,0,0} },
	{ ITEM, "id",			{ 0,0,0,0,0} },
	{ ITEM, "featured",	  	{ 0,0,0,0,0} },
	{ INCATEGORY, "\1",	  	{ UNIF,0,0,0,0} },
	{ PERSON, "id",		  	{ 0,0,0,0,0} },
	{ PROFILE, "income",	{ 0,0,0,0,0} },
	{ INTEREST, "\1",		{ UNIF,0,0,0,0} },
	{ WATCH, "\1",		   	{ UNIF,0,0,0,0} },
	{ OPEN_TRANS, "id",	  	{ 0,0,0,0,0} },
	{ ITEMREF, "\1",		{ UNIF,0,0,0,0} },
	{ PERSONREF, "\1",	   	{ UNIF,0,0,0,0} },
	{ SELLER, "\1",		  	{ GAUSS, 0.5, 0.1, 0, 0} },
	{ BUYER, "\1",		   	{ GAUSS, 0.5, 0.1, 0, 0} },
	{ AUTHOR, "\1",		  	{ UNIF,0,0,1,1} }
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

pub ProbDesc probDescForAttr(int element_id, char *attr_name) {
	for (size_t i = 0; i < nelem(attr_desc_map); i++) {
		attr_pd_item_t *item = &attr_desc_map[i];
		if (item->element_id == element_id && !strcmp(item->attr_name, attr_name)) {
			return item->p;
		}
	}
	panic("failed to find prob desc for attr '%s'", attr_name);
}


// char *dtd[] = {
//	 "<!ELEMENT site	(regions, categories, catgraph, people, open_auctions, closed_auctions)>",
//	 "<!ELEMENT categories	  (category+)>",
//	 "<!ELEMENT category		(name, description)>",
//	 "<!ATTLIST category		id ID #REQUIRED>",
//	 "<!ELEMENT name	(#PCDATA)>",
//	 "<!ELEMENT description	 (text | parlist)>",
//	 "<!ELEMENT text	(#PCDATA | bold | keyword | emph)*>",
//	 "<!ELEMENT bold		  (#PCDATA | bold | keyword | emph)*>",
//	 "<!ELEMENT keyword	  (#PCDATA | bold | keyword | emph)*>",
//	 "<!ELEMENT emph		  (#PCDATA | bold | keyword | emph)*>",
//	 "<!ELEMENT parlist	  (listitem)*>",
//	 "<!ELEMENT listitem		(text | parlist)*>",
//	 "",
//	 "<!ELEMENT catgraph		(edge*)>",
//	 "<!ELEMENT edge			EMPTY>",
//	 "<!ATTLIST edge			from IDREF #REQUIRED to IDREF #REQUIRED>",
//	 "",
//	 "<!ELEMENT regions (africa, asia, australia, europe, namerica, samerica)>",
//	 "<!ELEMENT africa  (item*)>",
//	 "<!ELEMENT asia	(item*)>",
//	 "<!ELEMENT australia	   (item*)>",
//	 "<!ELEMENT namerica		(item*)>",
//	 "<!ELEMENT samerica		(item*)>",
//	 "<!ELEMENT europe  (item*)>",
//	 "<!ELEMENT item	(location, quantity, name, payment, description, shipping, incategory+, mailbox)>",
//	 "<!ATTLIST item			id ID #REQUIRED",
//	 "						  featured CDATA #IMPLIED>",
//	 "<!ELEMENT location		(#PCDATA)>",
//	 "<!ELEMENT quantity		(#PCDATA)>",
//	 "<!ELEMENT payment (#PCDATA)>",
//	 "<!ELEMENT shipping		(#PCDATA)>",
//	 "<!ELEMENT reserve (#PCDATA)>",
//	 "<!ELEMENT incategory	  EMPTY>",
//	 "<!ATTLIST incategory	  category IDREF #REQUIRED>",
//	 "<!ELEMENT mailbox (mail*)>",
//	 "<!ELEMENT mail	(from, to, date, text)>",
//	 "<!ELEMENT from	(#PCDATA)>",
//	 "<!ELEMENT to	  (#PCDATA)>",
//	 "<!ELEMENT date	(#PCDATA)>",
//	 "<!ELEMENT itemref		 EMPTY>",
//	 "<!ATTLIST itemref		 item IDREF #REQUIRED>",
//	 "<!ELEMENT personref	   EMPTY>",
//	 "<!ATTLIST personref	   person IDREF #REQUIRED>",
//	 "",
//	 "<!ELEMENT people  (person*)>",
//	 "<!ELEMENT person  (name, emailaddress, phone?, address?, homepage?, creditcard?, profile?, watches?)>",
//	 "<!ATTLIST person		  id ID #REQUIRED>",
//	 "<!ELEMENT emailaddress	(#PCDATA)>",
//	 "<!ELEMENT phone   (#PCDATA)>",
//	 "<!ELEMENT address (street, city, country, province?, zipcode)>",
//	 "<!ELEMENT street  (#PCDATA)>",
//	 "<!ELEMENT city	(#PCDATA)>",
//	 "<!ELEMENT province		(#PCDATA)>",
//	 "<!ELEMENT zipcode (#PCDATA)>",
//	 "<!ELEMENT country (#PCDATA)>",
//	 "<!ELEMENT homepage		(#PCDATA)>",
//	 "<!ELEMENT creditcard	  (#PCDATA)>",
//	 "<!ELEMENT profile (interest*, education?, gender?, business, age?)>",
//	 "<!ATTLIST profile		 income CDATA #IMPLIED>",
//	 "<!ELEMENT interest		EMPTY>",
//	 "<!ATTLIST interest		category IDREF #REQUIRED>",
//	 "<!ELEMENT education	   (#PCDATA)>",
//	 "<!ELEMENT income  (#PCDATA)>",
//	 "<!ELEMENT gender  (#PCDATA)>",
//	 "<!ELEMENT business		(#PCDATA)>",
//	 "<!ELEMENT age	 (#PCDATA)>",
//	 "<!ELEMENT watches (watch*)>",
//	 "<!ELEMENT watch		   EMPTY>",
//	 "<!ATTLIST watch		   open_auction IDREF #REQUIRED>",
//	 "",
//	 "<!ELEMENT open_auctions   (open_auction*)>",
//	 "<!ELEMENT open_auction	(initial, reserve?, bidder*, current, privacy?, itemref, seller, annotation, quantity, type, interval)>",
//	 "<!ATTLIST open_auction	id ID #REQUIRED>",
//	 "<!ELEMENT privacy (#PCDATA)>",
//	 "<!ELEMENT initial (#PCDATA)>",
//	 "<!ELEMENT bidder  (date, time, personref, increase)>",
//	 "<!ELEMENT seller		  EMPTY>",
//	 "<!ATTLIST seller		  person IDREF #REQUIRED>",
//	 "<!ELEMENT current (#PCDATA)>",
//	 "<!ELEMENT increase		(#PCDATA)>",
//	 "<!ELEMENT type	(#PCDATA)>",
//	 "<!ELEMENT interval		(start, end)>",
//	 "<!ELEMENT start   (#PCDATA)>",
//	 "<!ELEMENT end	 (#PCDATA)>",
//	 "<!ELEMENT time	(#PCDATA)>",
//	 "<!ELEMENT status  (#PCDATA)>",
//	 "<!ELEMENT amount  (#PCDATA)>",
//	 "",
//	 "<!ELEMENT closed_auctions (closed_auction*)>",
//	 "<!ELEMENT closed_auction  (seller, buyer, itemref, price, date, quantity, type, annotation?)>",
//	 "<!ELEMENT buyer		   EMPTY>",
//	 "<!ATTLIST buyer		   person IDREF #REQUIRED>",
//	 "<!ELEMENT price   (#PCDATA)>",
//	 "<!ELEMENT annotation	  (author, description?, happiness)>",
//	 "",
//	 "<!ELEMENT author		  EMPTY>",
//	 "<!ATTLIST author		  person IDREF #REQUIRED>",
//	 "<!ELEMENT happiness	   (#PCDATA)>",
//	 NULL
// };