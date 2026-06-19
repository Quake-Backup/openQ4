
#ifndef __MD5_H__
#define __MD5_H__

/*
===============================================================================

	Calculates a checksum for a block of data
	using the MD5 message-digest algorithm.

===============================================================================
*/

unsigned long MD5_BlockChecksum( const void *data, int length );
bool MD5_FileChecksum( const char *path, char digestHex[33] );

#endif /* !__MD5_H__ */
