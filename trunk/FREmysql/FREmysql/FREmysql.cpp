// FREmysql.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "FlashRuntimeExtensions.h"

typedef int SOCKET;

#include "mysql.h"

extern "C"
{
	__declspec(dllexport) void initializer(void** extData, FREContextInitializer* ctxInitializer, FREContextFinalizer* ctxFinalizer);
	__declspec(dllexport) void finalizer(void* extData);
}

int avm_atom_isNull( FREObject o )
{
	FREObjectType ot;
	
	FREGetObjectType( o, &ot );
	
	return (ot==FRE_TYPE_NULL);
}

char *avm_atom_toCStr( FREObject o )
{
	uint32_t l;
	const uint8_t *v;

	if( avm_atom_isNull( o ) )
		return NULL;
	
	FREGetObjectAsUTF8( o, &l, &v );

	return (char*)v;
}

int32_t avm_atom_toInt( FREObject o )
{
	int32_t v;
	
	FREGetObjectAsInt32( o, &v );

	return v;
}

FREObject avm_intToAtom(int value)
{
	FREObject obj;
	
	FRENewObjectFromInt32( value, &obj );

	return obj;
}

FREObject avm_doubleToAtom(double value)
{
	FREObject obj;
	
	FRENewObjectFromDouble( value, &obj );

	return obj;
}

FREObject avm_String(char* value,int length)
{
	FREObject obj;

	if( length == -1 )
		length = strlen( value ) + 1;

	FRENewObjectFromUTF8( length, (uint8_t*)value, &obj );

	return obj;
}

int __query( MYSQL *m, char *q )
{
	if( mysql_real_query( m,q,strlen(q)) != 0 )
	{
		//error(MYSQLDATA(o),req);
		return -1;
	}

	return 0;
}

FREObject _query(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[])
{
	MYSQL *m;
	char* req = avm_atom_toCStr( argv[0] );

	FREGetContextNativeData( ctx, (void**)&m );
	
	if( __query( m,req) != 0 )
	{
		//error(MYSQLDATA(o),req);
		return NULL;
	}
		
	return NULL;
}

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

static FREObject _field( CONV fields_conv, char *row )
{
	FREObject v = NULL;

	switch( fields_conv ) 
	{
	case CONV_INT:
		v = avm_intToAtom(atoi(row));
		break;
	case CONV_STRING:
		v = avm_String(row,-1);
		break;
	case CONV_BOOL:
		v = avm_intToAtom( *row != '0' );
		break;
	case CONV_FLOAT:
		v = avm_doubleToAtom(atof(row));
		break;
	case CONV_BINARY:
		v = NULL;

		break;
	case CONV_DATE:
		
		v = NULL;
		
		break;
	case CONV_DATETIME:
		
		v = NULL;
		
		break;
	default:
		v = 0;
		break;
	}

	return v;

}

static FREObject _result( MYSQL_RES *r ) 
{
	int num_fields = mysql_num_fields(r);
	int i,j;
	
	MYSQL_FIELD *fields = mysql_fetch_fields(r);
	
	FREObject arr; 
		
	FRENewObject( (uint8_t*)"Array", 0, NULL, &arr, NULL );

	int num_rows = mysql_num_rows( r );

	for(j=0;j<num_rows;j++) 
	{
		MYSQL_ROW row = mysql_fetch_row( r );
		
		
		FREObject obj;
		FRENewObject( (uint8_t*)"Object", 0, NULL, &obj, NULL ); 
		
		for(i=0;i<num_fields;i++) 
		{
			char *fields_id = fields[i].name;
			CONV fields_conv = convert_type(fields[i].type,fields[i].length);

			FREObject val = _field( fields_conv, row[i] );

			FRESetObjectProperty( obj, (uint8_t*)fields_id, val, NULL );
		}
		
		FRESetArrayElementAt( arr, j, obj );
	}

	mysql_free_result(r);
	
	

	return arr;
}

FREObject _fetch(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[])
{
	MYSQL *m;
	char* req = avm_atom_toCStr( argv[0] );

	FREGetContextNativeData( ctx, (void**)&m );
	
	if( __query( m,req) != 0 )
	{
		//error(MYSQLDATA(o),req);
		return NULL;
	}

	MYSQL_RES *res;

	res = mysql_store_result( m );

	if( res == NULL ) 
	{
		if( mysql_field_count( m ) == 0 )
		{
			return 0;//avm_intToAtom( (int)mysql_affected_rows(MYSQLDATA(o)) );
		}
		else
		{
			//error(MYSQLDATA(o),req);
			return 0;

		}
	}
	
	return _result( res );
}

FREObject _selectdb(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[])
{
	MYSQL *m;

	FREGetContextNativeData( ctx, (void**)&m );

	if( mysql_select_db( m,avm_atom_toCStr(argv[0])) != 0 )
	{
		//throw error
		return NULL;
	}
	
	return NULL;
}

FREObject _connect(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[])
{
	FREObject o = argv[0];
	FREObject host, port, user, pass, socket;

	FREGetObjectProperty( o, (const uint8_t*)"host", &host, NULL );
	FREGetObjectProperty( o, (const uint8_t*)"port", &port, NULL );
	FREGetObjectProperty( o, (const uint8_t*)"user", &user, NULL );
	FREGetObjectProperty( o, (const uint8_t*)"pass", &pass, NULL );
	FREGetObjectProperty( o, (const uint8_t*)"socket",&socket, NULL );

	MYSQL *m;

	FREGetContextNativeData( ctx, (void**)&m );

	if( mysql_real_connect(m,avm_atom_toCStr(host),avm_atom_toCStr(user),avm_atom_toCStr(pass),NULL,avm_atom_toInt(port),avm_atom_isNull(socket)?NULL:avm_atom_toCStr(socket),0) == NULL ) 
	{
		//throw errro
		return NULL;
	}



}

void contextInitializer(void* extData, const uint8_t* ctxType, FREContext ctx, uint32_t* numFunctions, const FRENamedFunction** functions)
{
	*numFunctions = 4;

	FRENamedFunction* func = (FRENamedFunction*) malloc(sizeof(FRENamedFunction) * (*numFunctions));
	
	func[0].name = (const uint8_t*) "connect";
	func[0].functionData = NULL;
	func[0].function = &_connect;
	
	func[1].name = (const uint8_t*) "query";
	func[1].functionData = NULL;
	func[1].function = &_query;
	
	func[2].name = (const uint8_t*) "fetch";
	func[2].functionData = NULL;
	func[2].function = &_fetch;

	func[3].name = (const uint8_t*) "selectdb";
	func[3].functionData = NULL;
	func[3].function = &_selectdb;


	*functions = func;

	MYSQL *m = mysql_init(NULL);

	FRESetContextNativeData( ctx, m );
}

void contextFinalizer(FREContext ctx)
{
	MYSQL *m;

	FREGetContextNativeData( ctx, (void**)&m );

	mysql_close( m );
	
	return;
}

void initializer(void** extData, FREContextInitializer* ctxInitializer, FREContextFinalizer* ctxFinalizer)
{
	*ctxInitializer = &contextInitializer;
	*ctxFinalizer = &contextFinalizer;
}

void finalizer(void* extData)
{
	return;
}