package mysql
{
	import flash.display.BitmapData;
	import flash.external.ExtensionContext;
	import flash.utils.ByteArray;
	
	public class MySQL
	{
		private var context:ExtensionContext;
		
		public function MySQL()
		{
			context = ExtensionContext.createExtensionContext('mysql.MySQL', '');
		}
		
		public function connect(o:Object):void
		{
			context.call('connect', o );
		}		
		
		public function query(q:String):int
		{
			return context.call('query', q ) as int;
		}
		
		public function fetch(q:String):Array
		{
			return context.call('fetch', q ) as Array;
		}	
		
		public function selectdb(db:String):void
		{
			context.call('selectdb', db );
		}
		
	}
}