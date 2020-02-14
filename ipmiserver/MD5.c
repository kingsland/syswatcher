/* MD5C.C - RSA Data Security, Inc., MD5 message-digest algorithm
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD5 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD5 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.  

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.  

   These notices must be retained in any copies of any part of this
   documentation and/or software.  
 */

#include "Types.h"
#include "IPMIDefs.h"
#include "PMConfig.h"
#include <openssl/md5.h>

#define MD_CTX MD5_CTX
#define MDInit MD5_Init
#define MDUpdate MD5_Update
#define MDFinal MD5_Final

/*
INPUT  : Test string , buffer of 16 bytes to
store the digest,length of the string
OUTPUT	: Encrypted message in the buffer.
RETURN	: NONE
PROCESS	:Digests a string and prints the result.
 */
extern void AuthCodeCalMD5(
        _NEAR_ INT8U  *string, 	/*test string */
        _NEAR_ INT8U  *MD5Result,  /*result of MD5 digest*/
        INT16U LengthOfstring /*Length of input string */
        )
{
    MD_CTX context;
    INT8U  digest[16];
    MDInit (&context);
    MDUpdate (&context, string, LengthOfstring);
    MDFinal (digest, &context);
    _fmemcpy(MD5Result,digest,16);

}


