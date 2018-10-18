! ------------------------------------------------------------------------
! Pico Lingware building tools and sources
!
! Copyright (C) 2008-2009 SVOX AG, Baslerstr. 30, 8048 Zuerich, Switzerland
! All rights reserved.
!
! Prerequisites
!    perl, lua, cygwin 
! Tested with : 
!    perl 5.8.8   	http://www.perl.org/
!    lua  5.1.4 	http://www.lua.org/
!    cygwin 1.5.25	http://www.cygwin.com/	
!
! History:
! - 2009-05-30 -- initial version
!
! ------------------------------------------------------------------------

- unzip both packages on the same folder  

- on a cygwin console launch : 

./PicoLingware/Tools/buidlpkb.sh <langid>  <countryid>  <voice> 
     to build intermediate binary files from lingware sources  
     output will be in PicoLingware/pkb

./PicoLingware/Tools/buidlbin.sh <langcountryid> <speakerid> 
   to build final binary platform independent lingware resources from intermediate binaries 
   output will be in tools /PicoLingware/Tools
	