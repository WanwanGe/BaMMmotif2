#ifndef SEQUENCE_H_
#define SEQUENCE_H_

#include <cstring>	// e.g. std::memcpy
#include <vector>

#include <stdint.h>	// e.g. uint8_t
#include <math.h>
#include "Alphabet.h"

class Sequence{

public:

	Sequence( uint8_t* sequence, int L, std::string header, std::vector<int> Y, bool revcomp = false );
	~Sequence();

	uint8_t*        getSequence();
	int    			getL();
	std::string		getHeader();

	float 	        getIntensity();
	float 	        getWeight();

	void 	        setIntensity( float intensity );
	void 	        setWeight( float weight );

	int				extractKmer( int i, int k );	// extract (k+1)mer from positions (i-k,...,i) of the sequence

	void			print();						// print sequences

private:

	void 			appendRevComp( uint8_t* sequence, int L ); // append the sequence's reverse complement to the sequence

	uint8_t*		sequence_;						// sequence in alphabet encoding
	int				L_;								// sequence length
	std::string		header_;						// sequence header

	float			intensity_ = 0.0f;				// sequence intensity
	float			weight_ = 0.0f;					// sequence weight calculated from its intensity

	int* 			Y_;								// contains 1 at position 0
													// and the number of oligomers y for increasing order k at positions k+1
													// e.g.
													// alphabet size_ = 4: Y_ = 4^0 4^1 4^2 ... 4^15 < std::numeric_limits<int>::max()
													// limits the length of oligomers to 15 (and the order to 14)
};

inline int Sequence::extractKmer( int i, int k ){

	/**
	 *  extract (k+1)mer y from positions (i-k,...,i) of the sequence
	 *  e.g.		| monomer |                      dimer                      |      trimer     ... | ...
	 *  (k+1)mer:	| A C G T | AA AC AG AT CA CC CG CT GA GC GG GT TA TC TG TT | AAA AAC AAG AAT ... | ...
	 *  y:			| 0 1 2 3 |	 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 |  0   1   2   3  ... | ...
	 */
	int y = 0;

	for( int j = k; j >= 0; j-- ){
		// if unkown alphabets exist on the sequences, get a negative number for k-mer count y
		y += sequence_[i-j] > 0 ? ( sequence_[i-j] -1 ) * Y_[j] : -Y_[k+3];
	}
	return y;
}

#endif /* SEQUENCE_H_ */