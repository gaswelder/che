#import opt
#import rnd
#import ipsum.c
#import schema.c

typedef {
    schema.ProbDesc genref_pdnew;
} generator_state_t;

generator_state_t GLOBAL_STATE = {};

int stackdepth=0;
schema.Element *stack[64] = {};

const char *mapgen(int elem_id) {
	switch (elem_id) {
        case schema.CITY: { return "dict(cities)"; }
        case schema.STATUS, schema.HAPPINESS: { return "int(1, 10)"; }
        case schema.STREET: { return "street"; }
        case schema.PHONE: { return "phone"; }
        case schema.CREDITCARD: { return "cc"; }
        case schema.SHIPPING: { return "dict(shipping)"; }
        case schema.TIME: { return "time"; }
        case schema.AGE: { return "max(18, gauss(30, 15))"; }
        case schema.ZIPCODE: { return "zip"; }
        case schema.XDATE, schema.START, schema.END: { return "date"; }
        case schema.GENDER: { return "gender"; }
        case schema.AMOUNT, schema.PRICE: { return "price"; }
        case schema.TYPE: { return "type"; }
        case schema.LOCATION, schema.COUNTRY: { return "location"; }
        case schema.PROVINCE: { return "province"; }
        case schema.EDUCATION: { return "education"; }
        case schema.HOMEPAGE: { return "webpage"; }
        case schema.PAYMENT: { return "payment"; }
        case schema.BUSINESS, schema.PRIVACY: { return "yesno"; }
        case schema.CATNAME, schema.ITEMNAME: { return "sentence1"; }
        case schema.NAME: { return "name"; }
        case schema.FROM, schema.TO: { return "name "; }
        case schema.EMAIL: { return "email"; }
        case schema.QUANTITY: { return "quantity"; }
        case schema.INCREASE: { return "increase"; }
        case schema.CURRENT: { return "current"; }
        case schema.INIT_PRICE: { return "init_price"; }
        case schema.RESERVE: { return "reserve"; }
        case schema.TEXT: { return "any"; }
    }
	return NULL;
}

int main(int argc, char **argv) {
    float global_scale_factor = 1;
    opt.opt_float("f", "global_scale_factor", &global_scale_factor);
    opt.parse(argc, argv);

    schema.InitializeSchema(global_scale_factor);
    fprintf(stdout, "<?xml version=\"1.0\" standalone=\"yes\"?>\n");
    Tree(stdout, schema.GetSchemaNode(schema.nameid("site")));
    return 0;
}

void Tree(FILE *out, schema.Element *element) {
    if (element->type&0x10) {
        return;
    }
    OpeningTag(out, element);
    
    element->flag++;

	const char *spec = mapgen(element->id);
	if (spec != NULL) {
		ipsum.emit(spec);
		if (element->elm[0] != 0) {
        	fprintf(out,"\n");
    	}
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
        fprintf(stdout, "\n");
    }
    if ((element->att[0].name[0]) && element->elm[0] == 0) {
        return;
    }
    fprintf(stdout, "</%s>\n",element->name);
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
