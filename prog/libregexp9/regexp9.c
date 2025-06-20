// Copyright © 2021 Plan 9 Foundation

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#import utf

// Plan 9 regular expression notation

// Regular expression syntax used by the Plan 9 regular expression library.
// It is the form used by egrep before egrep got complicated.

// A regular expression specifies a set of strings of characters.
// In the following specification the word `character' means any character (rune) but newline.

// The syntax is

//        e3:  literal | charclass | '.' | '^' | '$' | '(' e0 ')'

//        e2:  e3
//         |  e2 REP

//        REP: '*' | '+' | '?'

//        e1:  e2
//               |  e1 e2

//        e0:  e1
//               |  e0 '|' e1

// A  literal  is  any  non-metacharacter,  or  a  metacharacter  (one  of
// .*+?[]()|\^$), or the delimiter preceded by

// A charclass is a nonempty string s bracketed [s] (or [^s]); it  matches
// any  character  in  (or  not  in)  s.   A negated character class never
// matches newline.  A substring a-b, with a and  b  in  ascending  order,
// stands  for  the  inclusive range of characters between a and b.  In s,
// the metacharacters an initial and the regular expression delimiter must
// be  preceded  by a other metacharacters have no special meaning and may
// appear unescaped.

// A matches any character.

// A matches the beginning of a line; matches the end of the line.

// The REP operators match zero or more (*), one or more (+), zero or  one
// (?), instances respectively of the preceding regular expression e2.

// A concatenated regular expression, e1e2, matches a match to e1 followed
// by a match to e2.

// An alternative regular expression, e0|e1, matches either a match to  e0
// or a match to e1.

// A  match to any part of a regular expression extends as far as possible
// without preventing a match to the remainder of the regular expression.



// Regcomplit is like
// regcomp except that all characters are treated literally.
// Regcompnl is like regcomp except that the "." metacharacter matches all characters, including newlines.

// Regexec matches a null-terminated
// string against the compiled regular expression in prog .
// If it matches, regexec returns 1 and fills in the array
// match with character pointers to the substrings of
// string that correspond to the
// parenthesized subexpressions of  exp :
// match[ i ].sp points to the beginning and match[ i ].ep points just beyond
// the end of the i th substring.
// (Subexpression i begins at the i th left parenthesis, counting from 1.)
// Pointers in match[0] pick out the substring that corresponds to the whole regular expression.
// Unused elements of match are filled with zeros. Matches involving *, +, and ?
// are extended as far as possible.
// The number of array elements in match is given by msize .
// The structure of elements of match is:

// If match[0].sp is nonzero on entry, regexec starts matching at that point within string .
// If match[0].ep is nonzero on entry, the last character matched is the one preceding that point.

// Regsub places in
// dest a substitution instance of
// source in the context of the last
// regexec performed using match .
// Each instance of \e n\f1,
// where
// n is a digit, is replaced by the
// string delimited by match[ n ].sp
// and match[ n ].ep\f1.
// Each instance of & is replaced by the string delimited by match[0].sp
// and match[0].ep.
// The substitution will always be null terminated and trimmed to fit into dlen bytes.

// Regerror, called whenever an error is detected in regcomp, writes the string
// msg on the standard error file and exits.
// Regerror can be replaced to perform
// special error processing.
// If the user supplied regerror returns rather than exits, regcomp will return 0. 

// Rregexec and rregsub are variants of regexec and regsub that use strings of Runes
// instead of strings of chars. With these routines, the rsp and rep fields of the
// match array elements should be used.

// Regcomp returns 0 for an illegal expression or other failure.
// Regexec returns 0 if string is not matched.

// There is no way to specify or match a NUL character; NULs terminate patterns and strings.


bool DEBUG = false;

/*
 *	Sub expression matches
 */
pub typedef {
	union
	{
		char *sp;
		utf.Rune *rsp;
	}s;
	union
	{
		char *ep;
		utf.Rune *rep;
	}e;
} Resub;

/*
 * Parser Information
 */
typedef {
	Reinst*	first;
	Reinst*	last;
} Node;

/*
 *  substitution list
 */
#define NSUBEXP 32
typedef  {
	Resub	m[NSUBEXP];
} Resublist;

typedef {
	Reinst*		inst;		/* Reinstruction of the thread */
	Resublist	se;		/* matched subexpressions in this thread */
} Relist;

typedef	{
	Relist*	relist[2];
	Relist*	reliste[2];
	int	starttype;
	utf.Rune	startchar;
	char*	starts;
	char*	eol;
	utf.Rune*	rstarts;
	utf.Rune*	reol;
} Reljunk;



/*
 *	character class, each pair of rune's defines a range
 */
pub typedef {
	utf.Rune	*end;
	utf.Rune	spans[64];
} Reclass;

/*
 *	Machine instructions
 */
pub typedef {
	int	type;
	union	{
		Reclass	*cp;		/* class pointer */
		utf.Rune	r;		/* character */
		int	subid;		/* sub-expression id for RBRA and LBRA */
		Reinst	*right;		/* right child of OR */
	}u1;
	union {	/* regexp relies on these two being in the same union */
		Reinst *left;		/* left child of OR */
		Reinst *next;		/* next instruction for CAT & LBRA */
	}u2;
} Reinst;

/*
 *	Reprogram definition
 */
#define MAX_CLASSES 16
pub typedef {
	Reinst	*startinst;	/* start pc */
	Reclass	class[MAX_CLASSES];	/* .data */
	Reinst	firstinst[5];	/* .text */
} Reprog;

/*
 * Actions and Tokens (Reinst types)
 *
 *	02xx are operators, value == precedence
 *	03xx are tokens, i.e. operands for operators
 */
enum {
	RUNE = 0177,
	OPERATOR = 0200, /* Bitmask of all operators */
	START = 0200, /* Start, used for marker on stack */
	RBRA = 0201, /* Right bracket, ) */
	LBRA = 0202, /* Left bracket, ( */
	OR = 0203, /* Alternation, | */
	CAT = 0204, /* Concatentation, implicit operator */
	STAR = 0205, /* Closure, * */
	PLUS = 0206, /* a+ == aa* */
	QUEST = 0207, /* a? == a|nothing, i.e. 0 or 1 a's */
	ANY = 0300, /* Any character except newline, . */
	ANYNL = 0301, /* Any character including newline, . */
	NOP = 0302, /* No operation, internal use only */
	BOL = 0303, /* Beginning of line, ^ */
	EOL = 0304, /* End of line, $ */
	CCLASS = 0305, /* Character class, [] */
	NCCLASS = 0306, /* Negated character class, [] */
	END = 0377	/* Terminate: match found */
}
// #define RUNE		0177
// #define	OPERATOR	0200	/* Bitmask of all operators */
// #define	START		0200	/* Start, used for marker on stack */
// #define	RBRA		0201	/* Right bracket, ) */
// #define	LBRA		0202	/* Left bracket, ( */
// #define	OR		0203	/* Alternation, | */
// #define	CAT		0204	/* Concatentation, implicit operator */
// #define	STAR		0205	/* Closure, * */
// #define	PLUS		0206	/* a+ == aa* */
// #define	QUEST		0207	/* a? == a|nothing, i.e. 0 or 1 a's */
// #define	ANY		0300	/* Any character except newline, . */
// #define	ANYNL		0301	/* Any character including newline, . */
// #define	NOP		0302	/* No operation, internal use only */
// #define	BOL		0303	/* Beginning of line, ^ */
// #define	EOL		0304	/* End of line, $ */
// #define	CCLASS		0305	/* Character class, [] */
// #define	NCCLASS		0306	/* Negated character class, [] */
// #define	END		0377	/* Terminate: match found */

/*
 *  regexec execution lists
 */
const int LISTSIZE = 10;
const int BIGLISTSIZE = 250; //	(25*LISTSIZE)

enum {
	NSTACK = 20
}
Node	andstack[NSTACK] = {};
Node	*andp = NULL;
int	atorstack[NSTACK] = {};
int*	atorp = NULL;
int	cursubid = 0;		/* id of current subexpression */
int	subidstack[NSTACK] = {};	/* parallel to atorstack */
int*	subidp = NULL;
int	lastwasand = 0;	/* Last token was operand */
int	nbra = 0;
char*	exprp = NULL;		/* pointer to next character in source expression */
int	lexdone = 0;
int	nclass = 0;

Reclass *classp = NULL;
/* max rune ranges per character class is nelem(classp->spans)/2 */


Reinst *freep = NULL;
int	errors = 0;
utf.Rune	yyrune = 0;		/* last lex'd rune */
Reclass *yyclassp = 0;	/* last lex'd class */

jmp_buf regkaboom = {};

Reprog *regcomp1(char *s, int literal, int dot_type) {
	int token = 0;

	Reprog *pp = calloc(sizeof(Reprog) + 6*sizeof(Reinst)*strlen(s), 1);
	if (!pp){
		regerror("out of memory");
		return 0;
	}
	freep = pp->firstinst;
	classp = pp->class;
	errors = 0;

	if(setjmp(regkaboom)) {
		if(errors){
			free(pp);
			pp = 0;
		}
		return pp;
	}

	/* go compile the sucker */
	lexdone = 0;
	exprp = s;
	nclass = 0;
	nbra = 0;
	atorp = atorstack;
	andp = andstack;
	subidp = subidstack;
	lastwasand = false;
	cursubid = 0;

	/* Start with a low priority operator to prime parser */
	pushator(START-1);
	while (true) {
		token = lex(literal, dot_type);
		if (token == END) {
			break;
		}
		if ((token & 0300) == OPERATOR) {
			operator(token);
		} else {
			operand(token);
		}
	}

	/* Close with a low priority operator */
	evaluntil(START);

	/* Force END */
	operand(END);
	evaluntil(START);
	if (DEBUG) {
		dumpstack();
	}
	if(nbra)
		rcerror("unmatched left paren");
	--andp;	/* points to first and only operand */
	pp->startinst = andp->first;
	if (DEBUG) {
		dump(pp);
	}
	pp = optimize(pp);
	if (DEBUG) {
		printf("start: %ld\n", andp->first - (Reinst *)pp->firstinst);
		dump(pp);
	}

	if(errors){
		free(pp);
		pp = 0;
	}
	return pp;
}



void
rcerror(char *s)
{
	errors++;
	regerror(s);
	longjmp(regkaboom, 1);
}

Reinst*
newinst(int t)
{
	freep->type = t;
	freep->u2.left = 0;
	freep->u1.right = 0;
	return freep++;
}

void
operand(int t)
{
	Reinst *i = NULL;

	if(lastwasand)
		operator(CAT);	/* catenate is implicit */
	i = newinst(t);

	if(t == CCLASS || t == NCCLASS)
		i->u1.cp = yyclassp;
	if(t == RUNE)
		i->u1.r = yyrune;

	pushand(i, i);
	lastwasand = true;
}

void
operator(int t)
{
	if(t==RBRA && --nbra<0)
		rcerror("unmatched right paren");
	if(t==LBRA){
		if(++cursubid >= NSUBEXP)
			rcerror ("too many subexpressions");
		nbra++;
		if(lastwasand)
			operator(CAT);
	} else
		evaluntil(t);
	if(t != RBRA)
		pushator(t);
	lastwasand = false;
	if(t==STAR || t==QUEST || t==PLUS || t==RBRA)
		lastwasand = true;	/* these look like operands */
}

void
regerr2(char *s, int c)
{
	char buf[100] = {};
	char *cp = buf;
	while(*s)
		*cp++ = *s++;
	*cp++ = c;
	*cp = '\0';
	rcerror(buf);
}

void
cant(char *s)
{
	char buf[100] = {};
	strcpy(buf, "can't happen: ");
	strcat(buf, s);
	rcerror(buf);
}

void
pushand(Reinst *f, Reinst *l)
{
	if(andp >= &andstack[NSTACK])
		cant("operand stack overflow");
	andp->first = f;
	andp->last = l;
	andp++;
}

void
pushator(int t)
{
	if(atorp >= &atorstack[NSTACK])
		cant("operator stack overflow");
	*atorp++ = t;
	*subidp++ = cursubid;
}

Node*
popand(int op)
{
	Reinst *inst = NULL;

	if(andp <= &andstack[0]){
		regerr2("missing operand for ", op);
		inst = newinst(NOP);
		pushand(inst,inst);
	}
	return --andp;
}

int
popator()
{
	if(atorp <= &atorstack[0])
		cant("operator stack underflow");
	--subidp;
	return *--atorp;
}

void evaluntil(int pri) {
	Node *op1 = NULL;
	Node *op2 = NULL;
	Reinst *inst1 = NULL;
	Reinst *inst2 = NULL;
	while (pri == RBRA || atorp[-1] >= pri) {
		switch (popator()) {
			case LBRA: {
				/* must have been RBRA */
				op1 = popand('(');
				inst2 = newinst(RBRA);
				inst2->u1.subid = *subidp;
				op1->last->u2.next = inst2;
				inst1 = newinst(LBRA);
				inst1->u1.subid = *subidp;
				inst1->u2.next = op1->first;
				pushand(inst1, inst2);
				return;
			}
			case OR: {
				op2 = popand('|');
				op1 = popand('|');
				inst2 = newinst(NOP);
				op2->last->u2.next = inst2;
				op1->last->u2.next = inst2;
				inst1 = newinst(OR);
				inst1->u1.right = op1->first;
				inst1->u2.left = op2->first;
				pushand(inst1, inst2);
			}
			case CAT: {
				op2 = popand(0);
				op1 = popand(0);
				op1->last->u2.next = op2->first;
				pushand(op1->first, op2->last);
			}
			case STAR: {
				op2 = popand('*');
				inst1 = newinst(OR);
				op2->last->u2.next = inst1;
				inst1->u1.right = op2->first;
				pushand(inst1, inst1);
			}
			case PLUS: {
				op2 = popand('+');
				inst1 = newinst(OR);
				op2->last->u2.next = inst1;
				inst1->u1.right = op2->first;
				pushand(op2->first, inst1);
			}
			case QUEST: {
				op2 = popand('?');
				inst1 = newinst(OR);
				inst2 = newinst(NOP);
				inst1->u2.left = inst2;
				inst1->u1.right = op2->first;
				op2->last->u2.next = inst2;
				pushand(inst1, inst2);
			}
			default: {
				rcerror("unknown operator in evaluntil");
			}
		}
	}
}

Reprog *optimize(Reprog *pp) {
	Reinst *inst = NULL;
	Reinst *target = NULL;
	Reclass *cl = NULL;

	/*
	 *  get rid of NOOP chains
	 */
	for (inst=pp->firstinst; inst->type!=END; inst++) {
		target = inst->u2.next;
		while (target->type == NOP)
			target = target->u2.next;
		inst->u2.next = target;
	}

	/*
	 *  The original allocation is for an area larger than
	 *  necessary.  Reallocate to the actual space used
	 *  and then relocate the code.
	 */
	size_t tmp = freep - (Reinst *)pp->firstinst;
	int size = sizeof(Reprog) + tmp * sizeof(Reinst);
	ptrdiff_t pp_pos = (char *)pp - (char *)NULL;
	Reprog *npp = realloc(pp, size);
	if (npp == NULL) {
		panic("realloc failed: %s", strerror(errno));
	}
	if (npp == pp) {
		return npp;
	}
	ptrdiff_t npp_pos = (char *)npp - (char *)NULL;
	ptrdiff_t diff = npp_pos - pp_pos;
	freep = (Reinst *)((char *)freep + diff);
	for (inst=npp->firstinst; inst<freep; inst++) {
		switch(inst->type){
			case OR, STAR, PLUS, QUEST: {
				inst->u1.right = (void*)((char*)inst->u1.right + diff);
			}
			case CCLASS, NCCLASS: {
				inst->u1.right = (void*)((char*)inst->u1.right + diff);
				cl = inst->u1.cp;
				cl->end = (void*)((char*)cl->end + diff);
			}
		}
		inst->u2.left = (void*)((char*)inst->u2.left + diff);
	}
	npp->startinst = (void*)((char*)npp->startinst + diff);
	return npp;
}

void dumpstack() {
	Node *stk = NULL;
	int *ip = NULL;

	printf("operators\n");
	for(ip=atorstack; ip<atorp; ip++)
		printf("0%o\n", *ip);
	printf("operands\n");
	for(stk=andstack; stk<andp; stk++)
		printf("0%o\t0%o\n", stk->first->type, stk->last->type);
}

void dump(Reprog *pp)
{
	Reinst *l = NULL;
	utf.Rune *p = NULL;

	l = pp->firstinst;
	while (true) {
		printf("%ld:\t0%o\t%ld\t%ld", l- (Reinst *)pp->firstinst, l->type,
			l->u2.left-pp->firstinst, l->u1.right-pp->firstinst);
		if(l->type == RUNE)
			printf("\t%d\n", l->u1.r);
		else if(l->type == CCLASS || l->type == NCCLASS){
			printf("\t[");
			if(l->type == NCCLASS)
				printf("^");
			for(p = l->u1.cp->spans; p < l->u1.cp->end; p += 2)
				if(p[0] == p[1])
					printf("%d", p[0]);
				else
					printf("%d-%d", p[0], p[1]);
			printf("]\n");
		} else
			printf("\n");
		if (!(l++->type)) {
			break;
		}
	}
}


Reclass* newclass() {
	if (nclass >= MAX_CLASSES) {
		rcerror("too many character classes; increase Reprog.class size");
	}
	return &(classp[nclass++]);
}

int nextc(utf.Rune *rp) {
	if (lexdone) {
		*rp = 0;
		return 1;
	}
	exprp += utf.get_rune(rp, exprp);
	if(*rp == '\\'){
		exprp += utf.get_rune(rp, exprp);
		return 1;
	}
	if(*rp == 0)
		lexdone = 1;
	return 0;
}

int lex(int literal, int dot_type) {
	int quoted = nextc(&yyrune);
	if (literal != 0 || quoted != 0) {
		if (yyrune == 0) {
			return END;
		}
		return RUNE;
	}

	switch (yyrune) {
		case 0:   { return END; }
		case '*': { return STAR; }
		case '?': { return QUEST; }
		case '+': { return PLUS; }
		case '|': { return OR; }
		case '.': { return dot_type; }
		case '(': { return LBRA; }
		case ')': { return RBRA; }
		case '^': { return BOL; }
		case '$': { return EOL; }
		case '[': { return bldcclass(); }
	}
	return RUNE;
}

int bldcclass() {
	utf.Rune r[nelem(classp->spans)] = {};
	utf.Rune *p = NULL;
	utf.Rune *ep = NULL;
	utf.Rune *np = NULL;
	utf.Rune rune = 0;
	int quoted = 0;

	/* we have already seen the '[' */
	int type = CCLASS;
	yyclassp = newclass();

	/* look ahead for negation */
	/* SPECIAL CASE!!! negated classes don't match \n */
	ep = r;
	quoted = nextc(&rune);
	if(!quoted && rune == '^'){
		type = NCCLASS;
		quoted = nextc(&rune);
		*ep++ = '\n';
		*ep++ = '\n';
	}

	/* parse class into a set of spans */
	while(ep < &r[nelem(classp->spans)-1]){
		if(rune == 0){
			rcerror("malformed '[]'");
			return 0;
		}
		if(!quoted && rune == ']')
			break;
		if(!quoted && rune == '-'){
			if(ep == r){
				rcerror("malformed '[]'");
				return 0;
			}
			quoted = nextc(&rune);
			if((!quoted && rune == ']') || rune == 0){
				rcerror("malformed '[]'");
				return 0;
			}
			*(ep-1) = rune;
		} else {
			*ep++ = rune;
			*ep++ = rune;
		}
		quoted = nextc(&rune);
	}
	if(ep >= &r[nelem(classp->spans)-1]) {
		rcerror("char class too large; increase Reclass.spans size");
		return 0;
	}

	/* sort on span start */
	for(p = r; p < ep; p += 2){
		for(np = p; np < ep; np += 2)
			if(*np < *p){
				rune = np[0];
				np[0] = p[0];
				p[0] = rune;
				rune = np[1];
				np[1] = p[1];
				p[1] = rune;
			}
	}

	/* merge spans */
	np = yyclassp->spans;
	p = r;
	if(r == ep)
		yyclassp->end = np;
	else {
		np[0] = *p++;
		np[1] = *p++;
		while (p < ep) {
			/* overlapping or adjacent ranges? */
			if(p[0] <= np[1] + 1){
				if(p[1] >= np[1])
					np[1] = p[1];	/* coalesce */
			} else {
				np += 2;
				np[0] = p[0];
				np[1] = p[1];
			}
			p += 2;
		}
		yyclassp->end = np+2;
	}

	return type;
}



// compiles a regular expression and returns
// a pointer to the generated description.
// The space is allocated by malloc and may be released by free.
pub Reprog* regcomp(char *s) {
	return regcomp1(s, 0, ANY);
}

pub Reprog* regcomplit(char *s) {
	return regcomp1(s, 1, ANY);
}

pub Reprog* regcompnl(char *s) {
	return regcomp1(s, 0, ANYNL);
}



/*
 *  save a new match in mp
 */
void
_renewmatch(Resub *mp, int ms, Resublist *sp)
{
	int i = 0;

	if (mp == NULL || ms<=0) {
		return;
	}
	if(mp[0].s.sp==0 || sp->m[0].s.sp<mp[0].s.sp ||
	   (sp->m[0].s.sp==mp[0].s.sp && sp->m[0].e.ep>mp[0].e.ep)){
		for(i=0; i<ms && i<NSUBEXP; i++)
			mp[i] = sp->m[i];
		while (i < ms) {
			mp[i].s.sp = mp[i].e.ep = 0;
			i++;
		}
	}
}

/*
 * Note optimization in _renewthread:
 * 	*lp must be pending when _renewthread called; if *l has been looked
 *		at already, the optimization is a bug.
 * lp is _relist to add to
 * ip is instruction to add
 * sep is pointers to subexpressions
 */
Relist* _renewthread(Relist *lp, Reinst *ip, int ms, Resublist *sep) {
	Relist *p = NULL;

	for (p = lp; p->inst; p++) {
		if (p->inst != ip) {
			continue;
		}
		if (sep->m[0].s.sp < p->se.m[0].s.sp) {
			if (ms > 1) {
				p->se = *sep;
			}
			else {
				p->se.m[0] = sep->m[0];
			}
		}
		return 0;
	}
	p->inst = ip;
	if (ms > 1) {
		p->se = *sep;
	}
	else {
		p->se.m[0] = sep->m[0];
	}
	(++p)->inst = 0;
	return p;
}

/*
 * same as renewthread, but called with
 * initial empty start pointer.
 */
 /* lp is _relist to add to */
 /* ip is instruction to add */
 /* sp is pointers to subexpressions */
Relist* _renewemptythread(Relist *lp, Reinst *ip, int ms, char *sp) {
	Relist *p = NULL;

	for(p=lp; p->inst; p++){
		if(p->inst == ip){
			if(sp < p->se.m[0].s.sp) {
				if(ms > 1)
					memset(&p->se, 0, sizeof(p->se));
				p->se.m[0].s.sp = sp;
			}
			return 0;
		}
	}
	p->inst = ip;
	if(ms > 1)
		memset(&p->se, 0, sizeof(p->se));
	p->se.m[0].s.sp = sp;
	(++p)->inst = 0;
	return p;
}

/* lp is _relist to add to */
/* ip is instruction to add */
/* rsp is pointers to subexpressions */
Relist*
_rrenewemptythread(Relist *lp,
	Reinst *ip,
	int ms,
	utf.Rune *rsp)
{
	Relist *p = NULL;

	for(p=lp; p->inst; p++){
		if(p->inst == ip){
			if(rsp < p->se.m[0].s.rsp) {
				if(ms > 1)
					memset(&p->se, 0, sizeof(p->se));
				p->se.m[0].s.rsp = rsp;
			}
			return 0;
		}
	}
	p->inst = ip;
	if(ms > 1)
		memset(&p->se, 0, sizeof(p->se));
	p->se.m[0].s.rsp = rsp;
	(++p)->inst = 0;
	return p;
}

void regerror(char *s) {
	fprintf(stderr, "regerror: %s\n", s);
	exit(1);
}

/*
 *  return	0 if no match
 *		>0 if a match
 *		<0 if we ran out of _relist space
 */
/* progp is program to run */
/* bol is string to run machine on */
/* mp is subexpression elements */
/* ms is number of elements at mp */
int
regexec1(Reprog *progp,
	char *bol,
	Resub *mp,
	int ms,
	Reljunk *j
)
{
	int flag=0;
	Reinst *inst;
	Relist *tlp;
	char *s;
	int i;
	int checkstart;
	utf.Rune r;
	utf.Rune *rp;
	utf.Rune *ep;
	int n;
	Relist* tl;		/* This list, next list */
	Relist* nl;
	Relist* tle;		/* ends of this and next list */
	Relist* nle;
	int match;
	char *p;

	match = 0;
	checkstart = j->starttype;
	if(mp)
		for(i=0; i<ms; i++) {
			mp[i].s.sp = 0;
			mp[i].e.ep = 0;
		}
	j->relist[0][0].inst = 0;
	j->relist[1][0].inst = 0;

	/* Execute machine once for each character, including terminal NUL */
	s = j->starts;
	while (true) {
		/* fast check for first char */
		if(checkstart) {
			switch(j->starttype) {
				case RUNE: {
					p = utf.utfrune(s, j->startchar);
					if (p == NULL || s == j->eol)
						return match;
					s = p;
				}
				case BOL: {
					if(s == bol)
						break;
					p = utf.utfrune(s, '\n');
					if (p == NULL || s == j->eol) {
						return match;
					}
					s = p+1;
				}
			}
		}
		r = *(uint8_t*)s;
		if(r < utf.Runeself)
			n = 1;
		else
			n = utf.get_rune(&r, s);

		/* switch run lists */
		tl = j->relist[flag];
		tle = j->reliste[flag];
		nl = j->relist[flag^=1];
		nle = j->reliste[flag];
		nl->inst = 0;

		/* Add first instruction to current list */
		if(match == 0)
			_renewemptythread(tl, progp->startinst, ms, s);

		/* Execute machine until current list is empty */
		for(tlp=tl; tlp->inst; tlp++){	/* assignment = */
			for(inst = tlp->inst; true; inst = inst->u2.next){
				switch(inst->type){
				case RUNE: {
					/* regular character */
					if(inst->u1.r == r){
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
					}
				}
				case LBRA: {
					tlp->se.m[inst->u1.subid].s.sp = s;
					continue;
				}
				case RBRA: {
					tlp->se.m[inst->u1.subid].e.ep = s;
					continue;
				}
				case ANY: {
					if(r != '\n')
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
				}
				case ANYNL: {
					if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
				}
				case BOL: {
					if(s == bol || *(s-1) == '\n')
						continue;
				}
				case EOL: {
					if(s == j->eol || r == 0 || r == '\n')
						continue;
				}
				case CCLASS: {
					ep = inst->u1.cp->end;
					for(rp = inst->u1.cp->spans; rp < ep; rp += 2)
						if(r >= rp[0] && r <= rp[1]){
							if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
								return -1;
							break;
						}
				}
				case NCCLASS: {
					ep = inst->u1.cp->end;
					for(rp = inst->u1.cp->spans; rp < ep; rp += 2)
						if(r >= rp[0] && r <= rp[1])
							break;
					if(rp == ep)
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
				}
				case OR: {
					/* evaluate right choice later */
					if(_renewthread(tlp, inst->u1.right, ms, &tlp->se) == tle)
						return -1;
					/* efficiency: advance and re-evaluate */
					continue;
				}
				case END: {
					/* Match! */
					match = 1;
					tlp->se.m[0].e.ep = s;
					if (mp != NULL) {
						_renewmatch(mp, ms, &tlp->se);
					}
				}}
				break;
			}
		}
		if (s == j->eol)
			break;
		checkstart = j->starttype && nl->inst == NULL;
		s += n;
		if (!r) {
			break;
		}
	}
	return match;
}

/* progp is program to run */
/* bol is string to run machine on */
/* mp is subexpression elements */
/* ms is number of elements at mp */
int
regexec2(Reprog *progp,
	char *bol,
	Resub *mp,
	int ms,
	Reljunk *j
)
{
	int rv;

	/* mark space */
	Relist *relist0 = calloc(BIGLISTSIZE, sizeof(Relist));
	if (relist0 == NULL) {
		return -1;
	}
	Relist *relist1 = calloc(BIGLISTSIZE, sizeof(Relist));
	if (relist1 == NULL) {
		free(relist0);
		return -1;
	}
	j->relist[0] = relist0;
	j->relist[1] = relist1;
	j->reliste[0] = relist0 + BIGLISTSIZE - 2;
	j->reliste[1] = relist1 + BIGLISTSIZE - 2;

	rv = regexec1(progp, bol, mp, ms, j);
	free(relist0);
	free(relist1);
	return rv;
}

/* program to run */
/* string to run machine on */
/* subexpression elements */
/* number of elements at mp */
pub int regexec(Reprog *progp, char *bol, Resub *mp, int ms) {

	Relist relist0[LISTSIZE];
	Relist relist1[LISTSIZE];
	int rv;

	/*
 	 *  use user-specified starting/ending location if specified
	 */
	Reljunk j;
	j.starts = bol;
	j.eol = 0;
	if(mp && ms>0){
		if(mp->s.sp)
			j.starts = mp->s.sp;
		if(mp->e.ep)
			j.eol = mp->e.ep;
	}
	j.starttype = 0;
	j.startchar = 0;
	if(progp->startinst->type == RUNE && progp->startinst->u1.r < utf.Runeself) {
		j.starttype = RUNE;
		j.startchar = progp->startinst->u1.r;
	}
	if(progp->startinst->type == BOL)
		j.starttype = BOL;

	/* mark space */
	j.relist[0] = relist0;
	j.relist[1] = relist1;
	j.reliste[0] = relist0 + nelem(relist0) - 2;
	j.reliste[1] = relist1 + nelem(relist1) - 2;

	rv = regexec1(progp, bol, mp, ms, &j);
	if(rv >= 0)
		return rv;
	rv = regexec2(progp, bol, mp, ms, &j);
	if(rv >= 0)
		return rv;
	return -1;
}

/* substitute into one string using the matches from the last regexec() */
/* sp is source string */
/* dp is destination string */
/* mp is subexpression elements */
/* ms is number of elements pointed to by mp */
pub void regsub(char *sp, *dp, int dlen, Resub *mp, int ms) {
	char *ssp;
	int i;
	char *ep = dp+dlen-1;

	while(*sp != '\0'){
		if (*sp == '\\') {
			switch(*++sp) {
			case '0', '1', '2', '3', '4', '5', '6', '7', '8', '9': {
				i = *sp-'0';
				if (mp != NULL && mp[i].s.sp != 0 && ms>i)
					for(ssp = mp[i].s.sp;
					     ssp < mp[i].e.ep;
					     ssp++)
						if(dp < ep)
							*dp++ = *ssp;
			}
			case '\\': {
				if(dp < ep)
					*dp++ = '\\';
			}
			case '\0': {
				sp--;
			}
			default: {
				if(dp < ep)
					*dp++ = *sp;
			}}
		} else if(*sp == '&') {
			if (mp != NULL && mp[0].s.sp != 0 && ms>0)
				for(ssp = mp[0].s.sp;
				     ssp < mp[0].e.ep; ssp++)
					if(dp < ep)
						*dp++ = *ssp;
		} else{
			if(dp < ep)
				*dp++ = *sp;
		}
		sp++;
	}
	*dp = '\0';
}



/*
 *  return	0 if no match
 *		>0 if a match
 *		<0 if we ran out of _relist space
 */
 /* program to run */
/* string to run machine on */
/* subexpression elements */
/* number of elements at mp */
int rregexec1(Reprog *progp,
	utf.Rune *bol,
	Resub *mp,
	int ms,
	Reljunk *j)
{
	int flag=0;
	Reinst *inst;
	Relist *tlp;
	utf.Rune *s;
	int i;
	int checkstart;
	utf.Rune r;
	utf.Rune *rp;
	utf.Rune *ep;
	Relist* tl;		/* This list, next list */
	Relist* nl;
	Relist* tle;		/* ends of this and next list */
	Relist* nle;
	int match;
	utf.Rune *p;

	match = 0;
	checkstart = j->startchar;
	if(mp)
		for(i=0; i<ms; i++) {
			mp[i].s.rsp = 0;
			mp[i].e.rep = 0;
		}
	j->relist[0][0].inst = 0;
	j->relist[1][0].inst = 0;

	/* Execute machine once for each character, including terminal NUL */
	s = j->rstarts;
	while (true) {
		/* fast check for first char */
		if(checkstart) {
			switch (j->starttype) {
			case RUNE: {
				p = utf.runestrchr(s, j->startchar);
				if(p == 0 || s == j->reol)
					return match;
				s = p;
			}
			case BOL: {
				if(s == bol)
					break;
				p = utf.runestrchr(s, '\n');
				if(p == 0 || s == j->reol)
					return match;
				s = p+1;
			}
			}
		}

		r = *s;

		/* switch run lists */
		tl = j->relist[flag];
		tle = j->reliste[flag];
		nl = j->relist[flag^=1];
		nle = j->reliste[flag];
		nl->inst = 0;

		/* Add first instruction to current list */
		_rrenewemptythread(tl, progp->startinst, ms, s);

		/* Execute machine until current list is empty */
		for(tlp=tl; tlp->inst; tlp++){
			for(inst=tlp->inst; true; inst = inst->u2.next){
				switch(inst->type){
				case RUNE: {
					/* regular character */
					if(inst->u1.r == r)
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
				}
				case LBRA: {
					tlp->se.m[inst->u1.subid].s.rsp = s;
					continue;
				}
				case RBRA: {
					tlp->se.m[inst->u1.subid].e.rep = s;
					continue;
				}
				case ANY: {
					if(r != '\n')
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
				}
				case ANYNL: {
					if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
				}
				case BOL: {
					if(s == bol || *(s-1) == '\n')
						continue;
				}
				case EOL: {
					if(s == j->reol || r == 0 || r == '\n')
						continue;
				}
				case CCLASS: {
					ep = inst->u1.cp->end;
					for(rp = inst->u1.cp->spans; rp < ep; rp += 2)
						if(r >= rp[0] && r <= rp[1]){
							if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
								return -1;
							break;
						}
				}
				case NCCLASS: {
					ep = inst->u1.cp->end;
					for(rp = inst->u1.cp->spans; rp < ep; rp += 2)
						if(r >= rp[0] && r <= rp[1])
							break;
					if(rp == ep)
						if(_renewthread(nl, inst->u2.next, ms, &tlp->se)==nle)
							return -1;
				}
				case OR: {
					/* evaluate right choice later */
					if(_renewthread(tlp, inst->u1.right, ms, &tlp->se) == tle)
						return -1;
					/* efficiency: advance and re-evaluate */
					continue;
				}
				case END: {
					/* Match! */
					match = 1;
					tlp->se.m[0].e.rep = s;
					if (mp != NULL) {
						_renewmatch(mp, ms, &tlp->se);
					}
				}}
				break;
			}
		}
		if (s == j->reol)
			break;
		checkstart = j->startchar != 0 && nl->inst == NULL;
		s++;
		if (!r) {
			break;
		}
	}
	return match;
}

/* program to run */
/* string to run machine on */
/* subexpression elements */
/* number of elements at mp */
int
rregexec2(Reprog *progp,
	utf.Rune *bol,
	Resub *mp,
	int ms,
	Reljunk *j
)
{
	Relist relist0[5*LISTSIZE];
	Relist relist1[5*LISTSIZE];

	/* mark space */
	j->relist[0] = relist0;
	j->relist[1] = relist1;
	j->reliste[0] = relist0 + nelem(relist0) - 2;
	j->reliste[1] = relist1 + nelem(relist1) - 2;

	return rregexec1(progp, bol, mp, ms, j);
}

/* program to run */
/* string to run machine on */
/* subexpression elements */
/* number of elements at mp */
pub int rregexec(Reprog *progp, utf.Rune *bol, Resub *mp, int ms) {
	Reljunk j;
	Relist relist0[LISTSIZE];
	Relist relist1[LISTSIZE];
	int rv;

	/*
 	 *  use user-specified starting/ending location if specified
	 */
	j.rstarts = bol;
	j.reol = 0;
	if(mp && ms>0){
		if(mp->s.sp)
			j.rstarts = mp->s.rsp;
		if(mp->e.ep)
			j.reol = mp->e.rep;
	}
	j.starttype = 0;
	j.startchar = 0;
	if(progp->startinst->type == RUNE && progp->startinst->u1.r < utf.Runeself) {
		j.starttype = RUNE;
		j.startchar = progp->startinst->u1.r;
	}
	if(progp->startinst->type == BOL)
		j.starttype = BOL;

	/* mark space */
	j.relist[0] = relist0;
	j.relist[1] = relist1;
	j.reliste[0] = relist0 + nelem(relist0) - 2;
	j.reliste[1] = relist1 + nelem(relist1) - 2;

	rv = rregexec1(progp, bol, mp, ms, &j);
	if(rv >= 0)
		return rv;
	rv = rregexec2(progp, bol, mp, ms, &j);
	if(rv >= 0)
		return rv;
	return -1;
}

/* substitute into one string using the matches from the last regexec() */
/* sp is source string */
/* dp is destination string */
/* mp is subexpression elements */
/* ms is number of elements pointed to by mp */
pub void rregsub(utf.Rune *sp, utf.Rune *dp, int dlen, Resub *mp, int ms) {
	utf.Rune *ssp;
	utf.Rune *ep;
	int i;

	ep = dp+(dlen/sizeof(utf.Rune))-1;
	while(*sp != '\0'){
		if(*sp == '\\'){
			switch(*++sp){
			case '0', '1', '2', '3', '4', '5', '6', '7', '8', '9': {
				i = *sp-'0';
				if( mp != NULL && mp[i].s.rsp != 0 && ms>i)
					for(ssp = mp[i].s.rsp;
					     ssp < mp[i].e.rep;
					     ssp++)
						if(dp < ep)
							*dp++ = *ssp;
			}
			case '\\': {
				if(dp < ep)
					*dp++ = '\\';
			}
			case '\0': {
				sp--;
			}
			default: {
				if(dp < ep)
					*dp++ = *sp;
			}
			}
		} else if (*sp == '&') {
			if (mp != NULL && mp[0].s.rsp != 0 && ms>0)
				for(ssp = mp[0].s.rsp;
				     ssp < mp[0].e.rep; ssp++)
					if (dp < ep) {
						*dp++ = *ssp;
					}
		} else {
			if (dp < ep) {
				*dp++ = *sp;
			}
		}
		sp++;
	}
	*dp = '\0';
}
