// This used to be xmlgen by Florian Waas (flw@mx4.org).

#import formats/dtd
#import ipsum
#import opt
#import rnd

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

const char *mapgen(const char *elemname) {
	if (!elemname) return NULL;
	switch str (elemname) {
        case "city": { return "dict(cities)"; }
		case "business", "privacy": { return "dict(yesno)"; }
        case "status", "happiness": { return "irand[1..10]"; }
        case "age": { return "irand[18..45]"; }
        case "amount", "price": { return "f2[10,100]"; }
        case "name": { return "text[1..1]"; }
        case "creditcard": { return "irand[1000..9999] ' ' irand[1000..9999] ' ' irand[1000..9999] ' ' irand[1000..9999]"; }
        case "current": { return "f2[1.5,115]"; }
        case "education": { return "dict(educations)"; }
        case "from", "to", "name": { return "dict(firstnames) ' ' dict(lastnames)"; }
        case "gender": { return "dict(genders)"; }
        case "homepage": { return "'https://' word '.' dict(domains)"; }
        case "increase": { return "f2[1.5,15]"; }
        case "initial": { return "f2[0,100]"; }
        case "location", "country": { return "dict(countries)"; }
        case "payment": { return "dict(payment_types)"; }
        case "phone": { return "'+' irand[1..99] ' (' irand[10..999] ') ' irand[123456..98765432]"; }
        case "province": { return "dict(provinces)"; }
        case "quantity": { return "irand[1..10]"; }
        case "reserve": { return "f2[50,250]"; }
        case "shipping": { return "dict(shipping)"; }
        case "street": { return "irand[1..100] ' ' dict(lastnames) ' st.'"; }
        case "time": { return "irand[0..23] ':' irand[0..59] ':' irand[0..59]"; }
        case "type": { return "dict(auction_types)"; }
        case "date", "start", "end": { return "irand[1..12] '/' irand[1..28] '/' irand[1998..2001]"; }
        case "zipcode": { return "irand[1000..9999]"; }
    }
	return "text[1..4]";
}

const int MAX_CHILDREN = 3;

char *auction_types[]={"Regular","Featured"};
char *genders[] = {"male", "female"};
char *yesno[] = {"yes", "no"};
char *educations[]={"High School","College", "Graduate School","Other"};
char *payment_types[]={"Money order","Creditcard", "Personal Check","Cash"};
char *SHIPPING[] = { "only within country", "internationally", "Buyer pays fixed shipping charges" };
char *domains[] = {
    "at", "au", "be", "bell-com", "ca", "ch", "cn", "com", "cz", "de", "dk", "edu",
    "fr", "gov", "gr", "hk", "ie", "in", "it", "jp", "kr", "net", "nl", "no", "org",
    "pl", "pt", "se", "uk",
};
char *provinces[]={
    "Alabama","Alaska","Arizona","Arkansas","California","Colorado",
    "Connecticut","Delaware","District Of Columbia","Florida","Georgia",
    "Hawaii","Idaho","Illinois","Indiana","Iowa","Kansas","Kentucky",
    "Louisiana","Maine","Maryland","Massachusetts","Michigan","Minnesota",
    "Mississipi","Missouri","Montana","Nebraska","Nevada","New Hampshire",
    "New Jersey","New Mexico","New York","North Carolina","North Dakota",
    "Ohio","Oklahoma","Oregon","Pennsylvania","Rhode Island",
    "South Carolina","South Dakota","Tennessee","Texas","Utah","Vermont",
    "Virginia","Washington","West Virginia","Wisconsin","Wyoming"
};
char *cities[]={
    "Abidjan","Abu","Acapulco","Aguascalientes","Akron","Albany",
    "Albuquerque","Alexandria","Allentown","Amarillo","Amsterdam",
    "Anchorage","Appleton","Aruba","Asheville","Athens","Atlanta",
    "Augusta","Austin","Baltimore","Bamako","Bangor","Barbados",
    "Barcelona","Basel","Baton","Beaumont","Berlin","Bermuda","Billings",
    "Birmingham","Boise","Bologna","Boston","Bozeman","Brasilia",
    "Brunswick","Brussels","Bucharest","Budapest","Buffalo","Butte",
    "Cairo","Calgary","Cancun","Cape","Caracas","Casper","Cedar",
    "Charleston","Charlotte","Charlottesville","Chattanooga","Chicago",
    "Chihuahua","Cincinnati","Ciudad","Cleveland","Cody","Colorado",
    "Columbia","Columbus","Conakry","Copenhagen","Corpus","Cozumel",
    "Dakar","Dallas","Dayton","Daytona","Denver","Des","Detroit","Dothan",
    "Dubai","Dublin","Durango","Durban","Dusseldorf","East","El","Elko",
    "Evansville","Fairbanks","Fayetteville","Florence","Fort","Fortaleza",
    "Frankfurt","Fresno","Gainesville","Geneva","George","Glasgow",
    "Gothenburg","Grand","Great","Green","Greensboro","Greenville",
    "Grenada","Guadalajara","Guangzhou","Guatemala","Guaymas","Gulfport",
    "Gunnison","Hamburg","Harrisburg","Hartford","Helena","Hermosillo",
    "Honolulu","Houston","Huntington","Huntsville","Idaho","Indianapolis",
    "Istanbul","Jackson","Jacksonville","Johannesburg","Kahului",
    "Kalamazoo","Kalispell","Kansas","Key","Kiev","Killeen","Knoxville",
    "La","Lafayette","Lansing","Las","Lawton","Leon","Lexington","Lima",
    "Lisbon","Little","Lome","London","Long","Lorient","Los","Louisville",
    "Lubbock","Lynchburg","Lyon","Macon","Madison","Madrid","Manchester",
    "Mazatlan","Melbourne","Memphis","Merida","Meridian","Mexico","Miami",
    "Milan","Milwaukee","Minneapolis","Missoula","Mobile","Monroe",
    "Monterrey","Montgomery","Montreal","Moscow","Mulhouse","Mumbai",
    "Munich","Myrtle","Nagoya","Nashville","Nassau","New","Newark",
    "Newburgh","Newcastle","Nice","Norfolk","Oakland","Oklahoma","Omaha",
    "Ontario","Orange","Orlando","Ouagadougou","Palm","Panama","Paris",
    "Pasco","Pensacola","Philadelphia","Phoenix","Pittsburgh","Pocatello",
    "Port","Portland","Porto","Prague","Providence","Providenciales",
    "Puebla","Puerto","Raleigh","Rapid","Reno","Richmond","Rio","Roanoke",
    "Rochester","Rome","Sacramento","Salt","Salvador","San","Santiago",
    "Sao","Sarasota","Savannah","Seattle","Shannon","Shreveport","South",
    "Spokane","St","Stockholm","Stuttgart","Sun","Syracuse","Tallahassee",
    "Tampa","Tapachula","Texarkana","Tokyo","Toledo","Toronto","Torreon",
    "Toulouse","Tri","Tucson","Tulsa","Turin","Twin","Vail","Valdosta",
    "Vancouver","Venice","Veracruz","Vienna","Villahermosa","Warsaw",
    "Washington","West","White","Wichita","Wilkes","Wilmington",
    "Windhoek","Worcester","Zihuatenejo","Zurich"
};
char *countries[]={
    "United States","Afghanistan","Albania","Algeria","American Samoa",
    "Andorra","Angola","Anguilla","Antarctica","Antigua","Argentina",
    "Armenia","Aruba","Australia","Austria","Azerbaijan","Bahamas",
    "Bahrain","Bangladesh","Barbados","Belarus","Belgium","Belize",
    "Benin","Bermuda","Bhutan","Bolivia","Botswana","Brazil",
    "British Indian Ocean Territory","British Virgin Islands",
    "Brunei Darussalam","Bulgaria","Burkina Faso","Burundi",
    "Cacos Islands","Cambodia","Cameroon","Canada","Cape Verde",
    "Cayman Islands","Central African Republic","Chad","Chile","China",
    "Christmas Island","Colombia","Comoros","Congo","Cook Islands",
    "Costa Rica","Croatia","Cuba","Cyprus","Czech Republic","Denmark",
    "Djibouti","Dominica","Dominican Republic","East Timor","Ecuador",
    "Egypt","El Salvador","Equatorial Guinea","Eritrea","Estonia",
    "Ethiopia","Falkland Islands","Faroe Islands","Fiji","Finland",
    "France","French Guiana","French Polynesia",
    "French Southern Territory","Futuna Islands","Gabon","Gambia",
    "Georgia","Germany","Ghana","Gibraltar","Greece","Greenland",
    "Grenada","Guadeloupe","Guam","Guatemala","Guinea","Guyana","Haiti",
    "Heard and Mcdonald Island","Honduras","Hong Kong","Hungary",
    "Iceland","India","Indonesia","Iran","Iraq","Ireland","Israel",
    "Italy","Ivory Coast","Jamaica","Japan","Jordan","Kazakhstan","Kenya",
    "Kiribati","Korea, Democratic People's Rep","Korea, Republic Of",
    "Kuwait","Kyrgyzstan","Lao People's Democratic Republ","Latvia",
    "Lebanon","Lesotho","Liberia","Libyan Arab Jamahiriya","Lithuania",
    "Luxembourg","Macau","Macedonia","Madagascar","Malawi","Malaysia",
    "Maldives","Mali","Malta","Marshall Islands","Martinique",
    "Mauritania","Mauritius","Mayotte","Mexico","Micronesia",
    "Moldova, Republic Of","Monaco","Mongolia","Montserrat","Morocco",
    "Mozambique","Myanmar","Namibia","Nauru","Nepal","Netherlands",
    "Netherlands Antilles","New Caledonia","New Zealand","Nicaragua",
    "Niger","Nigeria","Niue","Norfolk Island","Northern Mariana Islands",
    "Norway","Oman","Pakistan","Palau","Panama","Papua New Guinea",
    "Paraguay","Peru","Philippines","Poland","Portugal","Puerto Rico",
    "Qatar","Reunion","Romania","Russian Federation","Rwanda",
    "Saint Kitts","Samoa","San Marino","Sao Tome","Saudi Arabia",
    "Senegal","Seychelles","Sierra Leone","Singapore","Slovakia",
    "Slovenia","Solomon Islands","Somalia","South Africa","South Georgia",
    "Spain","Sri Lanka","St. Helena","St. Lucia","St. Pierre",
    "St. Vincent and Grenadines","Sudan","Suriname",
    "Svalbard and Jan Mayen Island","Swaziland","Sweden","Switzerland",
    "Syrian Arab Republic","Taiwan","Tajikistan","Tanzania","Thailand",
    "Togo","Tokelau","Tonga","Trinidad","Tunisia","Turkey","Turkmenistan",
    "Turks Islands","Tuvalu","Uganda","Ukraine","United Arab Emirates",
    "United Kingdom","Uruguay","Us Minor Islands","Us Virgin Islands",
    "Uzbekistan","Vanuatu","Vatican City State","Venezuela","Viet Nam",
    "Western Sahara","Yemen","Zaire","Zambia","Zimbabwe"
};
char *firstnames[] = {
	"Alice", "Barb", "Chris", "Drew", "Emily", "Flo", "Geoff", "Hugh", "Ilsa", "Jack",
    "Ken", "Laura", "Mark", "Nick", "Ophelia", "Peter", "Queen", "Richard", "Saul",
    "Terry", "Uncle", "Vito", "William", "Xenia", "Zohan",
};
char *lastnames[] = {
	"Apple", "Banana", "Chicken", "Dumpling", "Edible", "Fruit", "Granola", "Hummus",
    "Ice-Cream", "Jam", "Ketchup", "Lemon", "Mellon", "Noodle", "Orange", "Paprika",
    "Quesadilla", "Rice", "Spaghetti", "Toast", "Utkǎ", "Vinegar", "Waffle", "Yoghurt",
    "Zucchini",
};


int main(int argc, char **argv) {
    opt.parse(argc, argv);
	dtd.schema_t schema = dtd.parse(real_dtd);

	ipsum.add_dict("shipping", nelem(SHIPPING), SHIPPING);
    ipsum.add_dict("auction_types", nelem(auction_types), auction_types);
    ipsum.add_dict("cities", nelem(cities), cities);
    ipsum.add_dict("countries", nelem(countries), countries);
    ipsum.add_dict("domains", nelem(domains), domains);
    ipsum.add_dict("educations", nelem(educations), educations);
    ipsum.add_dict("firstnames", nelem(firstnames), firstnames);
    ipsum.add_dict("genders", nelem(genders), genders);
    ipsum.add_dict("lastnames", nelem(lastnames), lastnames);
    ipsum.add_dict("payment_types", nelem(payment_types), payment_types);
    ipsum.add_dict("provinces", nelem(provinces), provinces);
    ipsum.add_dict("yesno", nelem(yesno), yesno);

    fprintf(stdout, "<?xml version=\"1.0\" standalone=\"yes\"?>\n");
	emit_element(&schema, "site", 0);
    return 0;
}

// Emits a tree for element with the specified name.
void emit_element(dtd.schema_t *s, const char *name, int level) {
	if (level > 20) return;
	indent(level);

	dtd.element_t *e = dtd.get_element(s, name);
	if (!e) panic("element not found: %s", name);

	printf("<%s", name);
	dtd.attlist_t *a = dtd.get_attributes(s, name);
	if (a) {
		for (int i = 0; i < a->size; i++) {
			emit_attr(&a->items[i]);
		}
	}

	// If this element contains only text, format it in one line.
	if (children_only_cdata(e)) {
		printf(">");
		emit_child_list(s, e, level+1);
		printf("</%s>\n", name);
		return;
	}

	// If no children, format as void tag.
	if (e->children->size == 0) {
		printf(" />\n");
		return;
	}

	printf(">\n");
	emit_child_list(s, e, level + 1);
	indent(level);
	printf("</%s>\n", name);
}

void emit_attr(dtd.att_t *a) {
	printf(" %s=\"%d\"", a->name, rnd.intn(1000000));
}

// Returns true if element e's children are just pcdata.
bool children_only_cdata(dtd.element_t *e) {
	dtd.child_list_t *cl = e->children;
	if (cl->size != 1) return false;
	dtd.child_entry_t *ce = &e->children->items[0];
	if (ce->islist) return false;
	dtd.child_t *c = ce->data;
	return strcmp(c->name, "#PCDATA") == 0;
}

void emit_child_list(dtd.schema_t *s, dtd.element_t *e, int level) {
	dtd.child_list_t *l = e->children;
	int n = quant(l->quantifier);
	for (int j = 0; j < n; j++) {
		switch (l->jointype) {
			// \0 means the list has only one element.
			case '\0', ',': {
				// (a, b, c) -> emit all in sequence
				for (int i = 0; i < l->size; i++) {
					emit_child_entry(s, e->name, &l->items[i], level);
				}
			}
			case '|': {
				// (a | b | c) -> emit random one
				int i = rnd.intn(l->size);
				emit_child_entry(s, e->name, &l->items[i], level);
			}
			default: {
				panic("unimplemented jointype: %c", l->jointype);
			}
		}
	}
}

void emit_child_entry(dtd.schema_t *s, const char *parent_name, dtd.child_entry_t *ce, int level) {
	if (ce->islist) {
		panic("sublist not implemented");
	}
	dtd.child_t *c = ce->data;
	int n = quant(c->quantifier);
	for (int i = 0; i < n; i++) {
		if (strcmp(c->name, "#PCDATA") == 0) {
			ipsum.emit(mapgen(parent_name));
		} else {
			emit_element(s, c->name, level);
		}
	}
}

int quant(char q) {
	switch (q) {
		case '\0': { return 1; }
		case '*': { return rnd.intn(MAX_CHILDREN+1); }
		case '+': { return 1 + rnd.intn(MAX_CHILDREN); }
		case '?': { return rnd.intn(2); }
		default: {
			panic("unknown quantifier: %c", q);
		}
	}
}

void indent(int level) {
    for (int i = 0; i < level; i++) {
        putchar(' ');
		putchar(' ');
    }
}
