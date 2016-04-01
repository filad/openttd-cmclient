/* $Id: regression_info.nut 27192 2015-03-17 20:33:44Z frosch $ */

class Regression extends AIInfo {
	function GetAuthor()      { return "OpenTTD NoAI Developers Team"; }
	function GetName()        { return "Regression"; }
	function GetShortName()   { return "REGR"; }
	function GetDescription() { return "This runs regression-tests on some commands. On the same map the result should always be the same."; }
	function GetVersion()     { return 1; }
	function GetAPIVersion()  { return "1.6"; }
	function GetDate()        { return "2007-03-18"; }
	function CreateInstance() { return "Regression"; }
}

RegisterAI(Regression());

