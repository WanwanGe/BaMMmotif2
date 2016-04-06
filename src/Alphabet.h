/*
 * Alphabet.h
 *
 *  Created on: Apr 1, 2016
 *      Author: wanwan
 */

#ifndef ALPHABET_H_
#define ALPHABET_H_

#include <stdint.h>		/* uint8_t */
#include <stdlib.h>		/* calloc, exit, free */

class Alphabet {

public:

	static void 		init( char const* alphabetString );	// alphabetString defaults to ACGT but may later be extended to ACGTH(hydroxymethylcytosine) or similar
	static void 		destruct();													// free space

	static int 			getSize();													// get alphabet size
	static char* 		getAlphabet();											// get alphabet bases
  static char* 		getComplementAlphabet();						// get complementary alphabet bases
	static char			getCodeToBase( int code );					// get conversion from encoding to bases
	static uint8_t 	getBaseToCode( char base );					// get conversion from bases to encoding

private:

	static int 			size_;															// alphabet size
	static char* 		alphabet_;													// alphabet bases ([N,A,C,G,T], [N,A,C,G,T,mC], ...)
	static char* 		complementAlphabet_;								// complementary alphabet bases ([N,T,G,C,A,G], [N,T,G,C,A,G], ...)

	// conversion from encoding to bases
	static char* codeToBase = { 'N','A','C','G','T' }; 	// depending on alphabetString
	// conversion from bases to encoding
	static uint8_t baseToCode = ( uint8_t* )calloc( 127 * sizeof( uint8_t ) );
	baseToCode[( int )'A'] = 1
	baseToCode[( int )'C'] = 2
	baseToCode[( int )'G'] = 3
	baseToCode[( int )'T'] = 4 													// iterate over alphabetString
};

inline int 		Alphabet::getSize();
inline char* 	Alphabet::getAlphabet();
inline char* 	Alphabet::getComplementAlphabet();

inline char Alphabet::getCodeToBase( int code ){
	return codeToBase[code];
};

inline uint8_t Alphabet::getBaseToCode( char base ){
	return baseToCode[base];
};

#endif /* ALPHABET_H_ */
