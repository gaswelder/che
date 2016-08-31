#define MAX_KEY 256

struct list_element;

/* An element of the hash table. */
struct element
{
	//char key[MAX_KEY];
	int64 key;
	int value;
	struct element *next;
};

/* The table itself. */
struct hash_table
{
	size_t size; /* How many buckets there are. */
	size_t load; /* How many values are stored */
	struct element **buckets; /* Array of lists */
};

int hash_int64( int64 n )
{
	return n;
}

int hash_string( char *s )
{
	unsigned long h = 0;
	while( *s ) {
		h = (h * *s  + 2020181) % 2605013;
		s++;
	}
	return h;
}


/* Element creation */
struct element *create_element( int64 key, int value )
{
	struct element *e;
	
	e = calloc( 1, sizeof( struct element ) );
	//strcpy( e->key, key );
	e->key = key;
	e->value = value;
	
	return e;
}

/* Element removal. */
void free_element( struct element *e )
{
	free( e );
}


/* Table creation. */
htable *ht_create( size_t size )
{
	htable *t = calloc( 1, sizeof(htable) );
	
	t->size = size;
	t->load = 0;
	t->buckets = calloc( t->size, sizeof( struct element *) );
	return t;
}

/* Table removal. */
void ht_free( htable *t )
{
	struct element *cell;
	struct element *next;
	int i;
	int n;
	
	n = t->size;
	for( i = 0; i < n; i++ )
	{
		cell = t->buckets[i];
		while( cell )
		{
			next = cell->next;
			free_element( cell );
			cell = next;
		}
	}
	
	free( t->buckets );
	free( t );
}

void resize_table( htable *t )
{
	struct element **old_buckets;
	struct element *e;
	struct element *prev;
	int i;
	int n;
	
	/* Detach the buckets from the table. */
	old_buckets = t->buckets;
	n = t->size;
	
	/* Allocate new buckets. */
	t->size *= 2;
	t->buckets = calloc( t->size, sizeof( struct element *) );
	t->load = 0;
	
	/* Reinsert. */
	for( i = 0; i < n; i++ )
	{
		e = old_buckets[i];
		prev = NULL;
		while( e )
		{
			ht_set( t, e->key, e->value );
			prev = e;
			e = e->next;
			free_element( prev );
		}
	}
}


/* Finds the element for the given key. */
struct element *get_element( htable *t, int64 key, bool create )
{
	struct element *e;
	struct element *prev;
	int i;
	
	/* Get to the bucket. */
	//i = hash_string( key ) % t->size;
	i = hash_int64( key ) % t->size;
	e = t->buckets[i];
	
	/* Is the bucket empty? */
	if( !e ) {
		if( create ) {
			t->buckets[i] = create_element( key, 0 );
			t->load++;
			return t->buckets[i];
		}
		return e;
	}
	
	/* If not, go along the list and search the key. */
	while( 1 )
	{
		if( e->key == key ) {
			return e;
		} else {
			prev = e;
			e = e->next;
		}
		if( !e ) break;
	}

	if( create ) {
		prev->next = create_element( key, 0 );
		t->load++;
	}
	return prev->next;
}


/* Insert or update the value with the given key. */
void ht_set( htable *t, int64 key, int value )
{
	struct element *e;
	
	e = get_element( t, key, true );
	e->value = value;
	
	if( t->load >= t->size/2 ) {
		resize_table( t );
	}
}

/* Tells if the given key exists. */
int ht_isset( htable *t, int64 key )
{
	return get_element( t, key, false ) ? 1 : 0;
}

/* Returns value at the given key. */
int ht_get( htable *t, int64 key )
{
	struct element *e;
	
	e = get_element( t, key, false );
	return e->value;
}

void ht_foreach( htable *t,
	int (*f)( int64 key, int value )
)
{
	struct element *e;
	int i;
	int n;
	bool go = true;
	
	n = t->size;
	for( i = 0; i < n; i++ )
	{
		e = t->buckets[i];
		while( e )
		{
			if( f( e->key, e->value ) == 0 ) {
				go = false;
				break;
			}
			e = e->next;
		}
		if( !go ) break;
	}
}

void ht_print( htable *t )
{
	size_t i;
	struct element *cell;
	
	for( i = 0; i < t->size; i++ )
	{
		printf( "[%d]\n", i );
		cell = t->buckets[i];
		while( cell )
		{
			printf( "\t%I64d = %d\n", cell->key, cell->value );
			cell = cell->next;
		}
	}
	
	printf( "--\n" );
}
