/* ************************************************************************ */
/*																			*/
/*  Neko Standard Library													*/
/*  Copyright (c)2005 Nicolas Cannasse										*/
/*																			*/
/*  This program is free software; you can redistribute it and/or modify	*/
/*  it under the terms of the GNU General Public License as published by	*/
/*  the Free Software Foundation; either version 2 of the License, or		*/
/*  (at your option) any later version.										*/
/*																			*/
/*  This program is distributed in the hope that it will be useful,			*/
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of			*/
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the			*/
/*  GNU General Public License for more details.							*/
/*																			*/
/*  You should have received a copy of the GNU General Public License		*/
/*  along with this program; if not, write to the Free Software				*/
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */
/*																			*/
/* ************************************************************************ */


#include <stdlib.h>
#include <malloc.h>

#include <stdio.h>
#include <time.h>
typedef int SOCKET;
#include <mysql.h>
#include <string.h>

#include "avmrt.h"

/**
	<doc>
	<h1>MySQL</h1>
	<p>
	API to connect and use MySQL database
	</p>
	</doc>
**/

#define MYSQLDATA(o)	(MYSQL*)avm_Proxy_toVoid(o)
#define RESULT(o)		((result*)avm_Proxy_toVoid(o))

static void error( MYSQL *m, char *msg ) 
{
	printf(msg);
}

// ---------------------------------------------------------------
// Result

/**
	<doc><h2>Result</h2></doc>
**/

#undef CONV_FLOAT
typedef enum {
	CONV_INT,
	CONV_STRING,
	CONV_FLOAT,
	CONV_BINARY,
	CONV_DATE,
	CONV_DATETIME,
	CONV_BOOL
} CONV;

typedef struct {
	MYSQL_RES *r;
	int nfields;
	CONV *fields_convs;
	char **fields_ids;
	MYSQL_ROW current;
	Atom conv_date;
} result;

void __stdcall free_result( void *o ) 
{
	result *r = (result*)(o);

	mysql_free_result(r->r);
}

/**
	result_set_conv_date : 'result -> function:1 -> void
	<doc>Set the function that will convert a Date or DateTime string
	to the corresponding Atom.</doc>
**/
static Atom result_set_conv_date( Atom o, Atom c ) 
{
	//val_check_function(c,1);
	//if( val_is_int(o) )
	//	return val_true;
	//val_check_kind(o,k_result);
	//RESULT(o)->conv_date = c;
	//return val_true;

	return 0;

}

/**
	result_get_length : 'result -> int
	<doc>Return the number of rows returned or affected</doc>
**/
static Atom result_get_length( Atom o ) {
	//if( val_is_int(o) )
	//	return o;
	//val_check_kind(o,k_result);
	//return alloc_int( (int)mysql_num_rows(RESULT(o)->r) );
		return 0;
}

/**
	result_get_nfields : 'result -> int
	<doc>Return the number of fields in a result row</doc>
**/

__declspec( dllexport ) Atom __stdcall numrows(int argc, Atom* argv)
{
	result *r =RESULT(argv[1]);

	int n = mysql_num_rows( r->r );
	
	return avm_intToAtom( n );
}

/**
	result_next : 'result -> object?
	<doc>
	Return the next row if available. A row is represented
	as an object, which fields have been converted to the
	corresponding Neko Atom (int, float or string). For
	Date and DateTime you can specify your own conversion
	function using [result_set_conv_date]. By default they're
	returned as plain strings. Additionally, the TINYINT(1) will
	be converted to either true or false if equal to 0.
	</doc>
**/

#define PROXY(a,b) avm_Proxy(a,NULL,NULL,NULL,b);

static Atom result_next( Atom o ) 
{
	result *r;
	unsigned long *lengths = NULL;
	MYSQL_ROW row;

	//val_check_kind(o,k_result);

	r = RESULT(o);

	row = mysql_fetch_row(r->r);

	if( row == NULL )
		return NULL;
	
	{
		int i;
		Atom cur = avm_Object();

		r->current = row;

		for(i=0;i<r->nfields;i++)
		{
			if( row[i] != NULL ) 
			{
				Atom v;
				switch( r->fields_convs[i] ) 
				{
				case CONV_INT:
					v = avm_intToAtom(atoi(row[i]));
					break;
				case CONV_STRING:
					v = avm_String(row[i],-1);
					break;
				case CONV_BOOL:
					v = avm_intToAtom( *row[i] != '0' );
					break;
				case CONV_FLOAT:
					v = avm_doubleToAtom(atof(row[i]));
					break;
				case CONV_BINARY:
					if( lengths == NULL ) {
						lengths = mysql_fetch_lengths(r->r);
						//if( lengths == NULL )
							//val_throw(alloc_string("mysql_fetch_lengths"));
					}
					v = avm_Buffer(row[i],lengths[i]);
					break;
				case CONV_DATE:
					{
						struct tm t;
						sscanf(row[i],"%4d-%2d-%2d",&t.tm_year,&t.tm_mon,&t.tm_mday);

						v = avm_dayToAtom( t.tm_year, t.tm_mon-1, t.tm_mday );//row[i],-1);
					}
					break;
				case CONV_DATETIME:
					{
						struct tm t;
						sscanf(row[i],"%4d-%2d-%2d %2d:%2d:%2d",&t.tm_year,&t.tm_mon,&t.tm_mday,&t.tm_hour,&t.tm_min,&t.tm_sec);

						v = avm_dateToAtom( t.tm_year, t.tm_mon-1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, 0 );//row[i],-1);
					}
					break;
				default:
					v = 0;
					break;
				}

				avm_Object_setProperty(cur,r->fields_ids[i],v);
			}
		}

		avm_Object_setProperty(cur,"nfields", avm_intToAtom(r->nfields) );

		return cur;
	}

	return NULL;
}

/**
	result_get : 'result -> n:int -> string
	<doc>Return the [n]th field of the current row</doc>
**/

__declspec( dllexport ) Atom __stdcall fetch_row(int argc, Atom* argv)
{
	return result_next(argv[1]);
}

static CONV convert_type( enum enum_field_types t, unsigned int length ) 
{
	// FIELD_TYPE_TIMESTAMP
	// FIELD_TYPE_TIME
	// FIELD_TYPE_YEAR
	// FIELD_TYPE_NEWDATE
	// FIELD_TYPE_NEWDATE + 2: // 5.0 MYSQL_TYPE_BIT
	switch( t ) {
	case FIELD_TYPE_TINY:
		if( length == 1 )
			return CONV_BOOL;
	case FIELD_TYPE_SHORT:
	case FIELD_TYPE_LONG:
	case FIELD_TYPE_INT24:
		return CONV_INT;
	case FIELD_TYPE_LONGLONG:
	case FIELD_TYPE_DECIMAL:
	case FIELD_TYPE_FLOAT:
	case FIELD_TYPE_DOUBLE:
	case 246: // 5.0 MYSQL_NEW_DECIMAL
		return CONV_FLOAT;
	case FIELD_TYPE_BLOB:
	case FIELD_TYPE_TINY_BLOB:
	case FIELD_TYPE_MEDIUM_BLOB:
	case FIELD_TYPE_LONG_BLOB:
		return CONV_BINARY;
	case FIELD_TYPE_DATETIME:
		return CONV_DATETIME;
	case FIELD_TYPE_DATE:
		return CONV_DATE;
	case FIELD_TYPE_NULL:
	case FIELD_TYPE_ENUM:
	case FIELD_TYPE_SET:
	//case FIELD_TYPE_VAR_STRING:
	//case FIELD_TYPE_GEOMETRY:
	// 5.0 MYSQL_TYPE_VARCHAR
	default:
		return CONV_STRING;
	}

		return (CONV)0;
}

Atom  __stdcall get_numrows(void *o, const char *name)
{
	result *r = (result*)o;

	if( strcmp( name, "numrows" ) )
		return NULL;

	{
		int n = mysql_num_rows( r->r );
		
		return avm_intToAtom( n );
	}


}

static Atom alloc_result( MYSQL_RES *r ) 
{
	result *res = (result*)avm_gc_alloc(sizeof(result),GC_CONTAINSPOINTERS);
	
	Atom o;
	int num_fields = mysql_num_fields(r);
	int i,j;
	
	MYSQL_FIELD *fields = mysql_fetch_fields(r);
	
	res->r = r;
	res->conv_date = NULL;
	res->current = NULL;
	res->nfields = num_fields;
	res->fields_ids = (char**)avm_gc_alloc(sizeof(char*)*num_fields,0);
	res->fields_convs = (CONV*)avm_gc_alloc(sizeof(CONV)*num_fields,0);

	for(i=0;i<num_fields;i++) 
	{
		res->fields_ids[i] = fields[i].name;
		res->fields_convs[i] = convert_type(fields[i].type,fields[i].length);

	}
	
	o = avm_Proxy( res, get_numrows,0,0,free_result );

	return o;
}

// ---------------------------------------------------------------
// Connection

/** <doc><h2>Connection</h2></doc> **/

/**
	close : 'connection -> void
	<doc>Close the connection. Any subsequent operation will fail on it</doc>
**/
static Atom close( Atom o ) 
{
	/*val_check_kind(o,k_connection);
	mysql_close(MYSQLDATA(o));
	val_data(o) = NULL;
	val_kind(o) = NULL;
	val_gc(o,NULL);
	return val_true;*/

		return 0;
}

/**
	select_db : 'connection -> string -> void
	<doc>Select the database</doc>
**/
__declspec( dllexport ) Atom __stdcall select_db(int argc, Atom* argv)
{
	//val_check_kind(o,k_connection);
	//val_check(db,string);

	Atom o = argv[1];

	void *test = MYSQLDATA(o);

	if( mysql_select_db(MYSQLDATA(o),avm_atom_toCStr(argv[2])) != 0 )
		error(MYSQLDATA(o),"Failed to select database :");
	return avm_uintToAtom(1);

	return avm_uintToAtom(0);
}

/**
	request : 'connection -> string -> 'result
	<doc>Execute an SQL request. Exception on error</doc>
**/
__declspec( dllexport ) Atom __stdcall request(int argc, Atom* argv)
{
	MYSQL_RES *res;

	//val_check_kind(o,k_connection);
	//val_check(r,string);

	Atom o = argv[1];
	char *req = avm_atom_toCStr(argv[2]);

	if( mysql_real_query(MYSQLDATA(o),req,strlen(req)) != 0 )
		error(MYSQLDATA(o),req);

	res = mysql_store_result(MYSQLDATA(o));

	if( res == NULL ) 
	{
		if( mysql_field_count(MYSQLDATA(o)) == 0 )
			return avm_intToAtom( (int)mysql_affected_rows(MYSQLDATA(o)) );
		else
			error(MYSQLDATA(o),req);
	}

	return alloc_result(res);
}

/**
	escape : string -> string
	<doc>Escape the string for inserting into a SQL request</doc>
**/
static Atom escape( Atom o, Atom s ) 
{
	//int len;
	//Atom sout;
	//val_check_kind(o,k_connection);
	//val_check(s,string);
	//len = val_strlen(s) * 2;
	//sout = alloc_empty_string(len);
	//len = mysql_real_escape_string(MYSQLDATA(o),val_string(sout),val_string(s),val_strlen(s));
	//val_set_length(sout,len);
	//return sout;

		return 0;
}

// ---------------------------------------------------------------
// Sql


static void __stdcall free_connection( void *o ) 
{
	mysql_close((MYSQL*)o);
}

/**
	connect : { host => string, port => int, user => string, pass => string, socket => string? } -> 'connection
	<doc>Connect to a database using the connection informations</doc>
**/
__declspec( dllexport ) Atom __stdcall _connect(int argc, Atom* argv)
{
	Atom o = argv[1];
	
	Atom host, port, user, pass, socket;

	host = avm_Object_getProperty( o, "host" );
	port = avm_Object_getProperty( o, "port" );
	user = avm_Object_getProperty( o, "user" );
	pass = avm_Object_getProperty( o, "pass" );
	socket = avm_Object_getProperty( o, "socket" );

//	if( !val_is_string(socket) && !val_is_null(socket) )
//		neko_error();
	{
		MYSQL *m = mysql_init(NULL);

		if( mysql_real_connect(m,avm_atom_toCStr(host),avm_atom_toCStr(user),avm_atom_toCStr(pass),NULL,avm_atom_toInt(port),avm_atom_isNull(socket)?NULL:avm_atom_toCStr(socket),0) == NULL ) 
		{
			error(m,"no connect");
			return NULL;
		}

		return PROXY( m, free_connection );
	}

	return 0;
}

/* ************************************************************************ */
