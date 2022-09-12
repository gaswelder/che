enum {
    CATNAME,
    EMAIL,
    PHONE,
    STREET,
    CITY,
    COUNTRY,
    ZIPCODE,
    PROVINCE,
    HOMEPAGE,
    EDUCATION,
    GENDER,
    BUSINESS,
    NAME,
    AGE,
    CREDITCARD,
    LOCATION,
    QUANTITY,
    PAYMENT,
    SHIPPING,
    FROM, TO,
    XDATE,
    ITEMNAME,
    TEXT,
    AMOUNT,
    CURRENT,
    INCREASE,
    RESERVE,
    MAILBOX, BIDDER, PRIVACY, ITEMREF, SELLER, TYPE, TIME,
    STATUS, PERSONREF, INIT_PRICE, START, END, BUYER, PRICE, ANNOTATION,
    HAPPINESS, AUTHOR
};

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
char *GenContents_shipping[]={
                "Will ship only within country",
                "Will ship internationally",
                "Buyer pays fixed shipping charges",
                "See description for charges"};

pub int GenContents(FILE *xmlout, int type) {
    int r,i,result=1;
    switch(type) {
        case CITY:
            fprintf(xmlout,cities[ignuin(0,cities_len-1)]);
            break;
        case TYPE:
            fprintf(xmlout,GenContents_auction_type[ignuin(0,1)]);
            if (GenContents_quantity>1 && ignuin(0,1)) fprintf(xmlout,", Dutch");
            break;
        case LOCATION:
        case COUNTRY:
            if (genunf(0,1)<0.75) GenContents_country=countries_USA;
            else GenContents_country=ignuin(0,countries_len-1);
            fprintf(xmlout,countries[GenContents_country]);
            break;
        case PROVINCE:
            if (GenContents_country==countries_USA)
                fprintf(xmlout,provinces[ignuin(0,provinces_len-1)]);
            else
                fprintf(xmlout,lastnames[ignuin(0,lastnames_len-1)]);
            break;
        case EDUCATION:            
            fprintf(xmlout,GenContents_education[ignuin(0,3)]);
            break;
        case STATUS:
        case HAPPINESS:
            fprintf(xmlout,"%d",ignuin(1,10));
            break;
        case HOMEPAGE:
            fprintf(xmlout,"http://www.%s/~%s", emails[GenContents_email],lastnames[GenContents_lstname]);
            break;
        case STREET:
            r=ignuin(0,lastnames_len-1);
            fprintf(xmlout,"%d %s St",ignuin(1,100),lastnames[r]);
            break;
        case PHONE:
            int contry=ignuin(1,99);
            int area=ignuin(10,999);
            int number=ignuin(123456,98765432);
            fprintf(xmlout,"+%d (%d) %d",country,area,number);
            break;
        case CREDITCARD:
            for(int i=0;i<4;i++) {
                char *s = "";
                if (i < 3) {
                    s = " ";
                }
                fprintf(xmlout,"%d%s",ignuin(1000,9999), s);
            }
            break;
        case PAYMENT:
            r=0;
            for (int i=0;i<4;i++)
                if (ignuin(0,1)) {
                    char *x = "";
                    if (r++) {
                        x = ", ";
                    }
                    fprintf(xmlout, "%s%s", x, GenContents_money[i % 4]);
                }
            break;
        case SHIPPING:
            r=0;
            for (int i=0;i<4;i++)
                if (ignuin(0,1)) {
                    char *s = "";
                    if (r++) s = ", ";
                    fprintf(xmlout,"%s%s",s,GenContents_shipping[i % 4]);
                }
            break;
        case TIME:
            int hrs=ignuin(0,23);
            int min=ignuin(0,59);
            int sec=ignuin(0,59);
            fprintf(xmlout,"%02d:%02d:%02d",hrs,min,sec);
            break;
        case AGE:
            r=(int)gennor(30,15);
            if (r < 18) r = 18;
            fprintf(xmlout,"%d", r);
            break;
        case ZIPCODE:
            r=ignuin(3,4);
            int cd=10;
            for (int j=0;j<r;j++) cd*=10;
            r=ignuin(j,(10*j)-1);
            fprintf(xmlout,"%d",r);
            break;
        case BUSINESS:
        case PRIVACY:
            fprintf(xmlout,GenContents_yesno[ignuin(0,1)]);
            break;
        case XDATE:
        case START:
        case END:
            int month=ignuin(1,12);
            int day=ignuin(1,28);
            int year=ignuin(1998,2001);
            fprintf(xmlout,"%02d/%02d/%4d",month,day,year);
            break;
        case CATNAME:
        case ITEMNAME:
            PrintSentence(ignuin(1,4));
            break;
        case NAME:
            PrintName(&GenContents_lstname);
            break;
        case FROM:
        case TO:
            PrintName(&GenContents_lstname);
            fprintf(xmlout," ");
        case EMAIL:
            GenContents_email=ignuin(0,emails_len-1);
            fprintf(xmlout,"mailto:%s@%s",lastnames[GenContents_lstname],emails[GenContents_email]);
            break;
        case GENDER:
            r=ignuin(0,1);
            if (r < 1) {
                fprintf(xmlout,"%s","male");
            } else {
                fprintf(xmlout,"%s","female");
            }
            break;
        case QUANTITY:
            GenContents_quantity=1+(int)genexp(0.4);
            fprintf(xmlout,"%d",GenContents_quantity);
            break;
        case INCREASE:
            double d=1.5 *(1+(int)genexp(10));
            fprintf(xmlout,"%.2f",d);
            GenContents_increases+=d;
        break;
        case CURRENT:
            fprintf(xmlout,"%.2f",GenContents_initial+GenContents_increases);
            break;
        case INIT_PRICE:
            GenContents_initial=genexp(100);
            GenContents_increases=0;
            fprintf(xmlout,"%.2f",GenContents_initial);
            break;
        case AMOUNT:
        case PRICE:
            fprintf(xmlout,"%.2f",genexp(100));
            break;
        case RESERVE:
            fprintf(xmlout,"%.2f",GenContents_initial*(1.2+genexp(2.5)));
            break;
        case TEXT:
            PrintANY();
            break;
        default:
            result=0;
        }
    return result;
}
