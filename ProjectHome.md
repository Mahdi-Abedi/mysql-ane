Flash, ActionScript, ActionScript, Air.

mysql database drive fo as3 - Flash Runtime Extension (mysql.ane)

for ActionScript3 (windows-x86)

Native mysql database client for AIR3

Usage:

1) add to application XML file:

> 

&lt;supportedProfiles&gt;

extendedDesktop

&lt;/supportedProfiles&gt;



> 

&lt;extensions&gt;


> > 

&lt;extensionID&gt;

mysql.MySQL

&lt;/extensionID&gt;



> 

&lt;/extensions&gt;



2) Put mysqle.ane to /extensions

3) Pack application with -extdir extensions
> (or run adl -extdir extensions)