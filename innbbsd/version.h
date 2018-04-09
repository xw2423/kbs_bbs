/*  Version information */
/*
$Id: version.h 5903 2004-01-05 07:13:28Z kcn $
*/

#ifndef _VERSION_H_
#define _VERSION_H_

#ifndef INNBBSDVERSION
#  define INNBBSDVERSION	"0.50beta-5F"
#endif

#ifndef NCMVERSION
#  define NCMVERSION    	"NoCeM_0.63"
#endif

#ifdef USE_NCM_PATCH
#undef VERSION
#  define VERSION       	INNBBSDVERSION"_"NCMVERSION
#else
#undef VERSION
#  define VERSION       	INNBBSDVERSION
#endif

#endif
