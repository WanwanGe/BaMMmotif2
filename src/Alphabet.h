/*
 * Alphabet.h
 *
 *  Created on: Apr 1, 2016
 *      Author: wanwan
 */

#ifndef ALPHABET_H_
#define ALPHABET_H_

#include <stdint.h>		// uint8_t

class Alphabet {

public:

	static void 		init( char* alphabetType );
	static void 		destruct();					        // free space

	static int 			getSize();							// get alphabet size
	static char const* 	getAlphabet();						// get alphabet bases
	static char const* 	getComplementAlphabet();			// get complementary alphabet bases
	static uint8_t 	    getCode( char base );				// get conversion from base to encoding
	static char 	    getBase( uint8_t code );			// get conversion from encoding to base
	static uint8_t 	    getComplementCode( uint8_t code );	// get conversion from encoding to complement encoding

	static void 		setSize( int size);

private:

	static int 			size_;							    // alphabet size, count the number of letters
	static char const* 	alphabet_;							// alphabet bases ([N,A,C,G,T], [N,A,C,G,T,mC], ...)
	static char const* 	complementAlphabet_;				// complementary alphabet bases ([N,T,G,C,A,G], [N,T,G,C,A,G], ...)
	static uint8_t* 	baseToCode_;						// conversion from base to encoding
	static char* 		codeToBase_;						// conversion from encoding to base
	static uint8_t* 	codeToComplementCode_;				// conversion from encoding to complement encoding
};

inline int Alphabet::getSize(){
	return size_;
}

inline uint8_t Alphabet::getCode( char base ){
	return baseToCode_[( int )base];
}

inline char Alphabet::getBase( uint8_t code ){
	return codeToBase_[ code ];
}

inline uint8_t Alphabet::getComplementCode( uint8_t code ){
	return codeToComplementCode_[code];
}

#endif /* ALPHABET_H_ */
