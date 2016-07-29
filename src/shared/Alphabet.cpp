/*
 * Alphabet.cpp
 *
 *  Created on: Apr 12, 2016
 *      Author: wanwan
 */

#include "Alphabet.h"

int 			Alphabet::size_;					// alphabet size, count the number of letters
char const* 	Alphabet::alphabet_;				// alphabet bases ([N,A,C,G,T], [N,A,C,G,T,mC], ...)
char const* 	Alphabet::complementAlphabet_;		// complementary alphabet bases ([N,T,G,C,A,G], [N,T,G,C,A,G], ...)
uint8_t* 		Alphabet::baseToCode_;				// conversion from bases to encoding
char* 			Alphabet::codeToBase_;				// conversion from encoding to base
uint8_t* 		Alphabet::codeToComplementCode_;	// conversion from encoding to complement encoding

void Alphabet::init( char* alphabetType ){

	if( strcmp( alphabetType, "STANDARD" ) == 0 ){
		size_ = 4;
		alphabet_ = "ACGT";
		complementAlphabet_ = "TGCA";
	} else if( strcmp( alphabetType, "METHYLCY" ) == 0 ){
		size_ = 5;
		alphabet_ = "ACGTM";
		complementAlphabet_ = "TGCAG";
	} else if( strcmp( alphabetType, "HYDRMETC" ) == 0 ){
		size_ = 5;
		alphabet_ = "ACGTH";
		complementAlphabet_ = "TGCAG";
	} else if( strcmp( alphabetType, "EXTENDED" ) == 0 ){
		size_ = 6;
		alphabet_ = "ACGTMH";
		complementAlphabet_ = "TGCAGG";
	} else {
		fprintf( stderr, "Please provide one of the following alphabet types: "
		        "STANDARD, METHYLCY, HYDRMETC, EXTENDED.\n" );
		exit( -1 );
	}

	baseToCode_ = ( uint8_t* )calloc( 128, sizeof( uint8_t ) );
	codeToBase_ = ( char* )calloc( 128, sizeof( char ) );
	for( int i = 0; i < size_; i++ ){
	  	// base:	A	C	T	G	...		N
	  	// code:	1	2	3	4	...		0
	  	baseToCode_[( int )alphabet_[i]] = ( uint8_t )( i + 1 );
	  	baseToCode_[( int )tolower( alphabet_[i] )] = ( uint8_t )( i + 1 );		// for lower case
	  	codeToBase_[i+1] = alphabet_[i];
	}

	codeToComplementCode_ = ( uint8_t* )calloc( size_+1, sizeof( uint8_t ) );
	for( int i = 0; i < size_; i++ )
		codeToComplementCode_[i+1] = baseToCode_[( int )complementAlphabet_[i]];
}

void Alphabet::destruct(){
	// free memory allocated to the pointers
	if( baseToCode_ ) free( baseToCode_ );
	if( codeToBase_ ) free( codeToBase_ );
	if( codeToComplementCode_ ) free( codeToComplementCode_ );
	std::cout << "Destruct() for Alphabet class works fine. \n";
}

char const* Alphabet::getAlphabet(){
	return alphabet_;
}

char const* Alphabet::getComplementAlphabet(){
	return complementAlphabet_;
}

void Alphabet::setSize( int size ){
	size_ = size;
}
