/*
 * Global.h
 *
 *  Created on: Apr 1, 2016
 *      Author: wanwan
 */

#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <iostream>											// std::cout, std::endl
#include <vector>
#include <list>
#include <string>
#include <cstring> 											// std::strcpy
#include <numeric>											// std::iota
#include <iomanip>											// std::setprecision
#include <fstream>
#include <limits>											// std::numeric_limits<int>::max();

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../shared/SequenceSet.h"
#include "../shared/Alphabet.h"
#include "../getopt_pp/getopt_pp.h"							// GetOpt function

class Global{

public:

	static char*		outputDirectory; 					// output directory

	static char*		posSequenceFilename;				// filename of positive sequence FASTA file
	static char*		negSequenceFilename;				// filename of negative sequence FASTA file

	static char*		posSequenceBasename;				// basename of positive sequence FASTA file
	static char*		negSequenceBasename;				// basename of negative sequence FASTA file


	static char* 		alphabetType;						// provide alphabet type
	static bool			revcomp;							// also search on reverse complement of sequences, defaults to false

	static SequenceSet*	posSequenceSet;						// positive Sequence Set
	static SequenceSet*	negSequenceSet;						// negative Sequence Set

	static char*		intensityFilename;					// filename of intensity file (i.e. for HT-SELEX data)
	// further weighting options...

	// files to initialize model(s)
	static char*		BaMMpatternFilename;				// filename of BaMMpattern file
	static char*		bindingSiteFilename;				// filename of binding sites file
	static char*		PWMFilename;						// filename of PWM file
	static char*		BaMMFilename;						// filename of Markov model (.bmm) file

	static char*		initialModelBasename;				// basename of initial model

	// model options
	static int			modelOrder;							// model order
	static std::vector<float> modelAlpha;					// initial alphas
	static std::vector<int>	addColumns;						// add columns to the left and right of models used to initialize Markov models

	// background model options
	static int			bgModelOrder;						// background model order, defaults to 2
	static float		bgModelAlpha;						// background model alpha

	// EM options
	static unsigned int	maxEMIterations;					// maximum number of iterations
	static float		epsilon;							// threshold for likelihood convergence parameter
	static bool			noAlphaOptimization;				// disable alpha optimization
	static bool			noQOptimization;					// disable q optimization

	// FDR options
	static bool			FDR;								// triggers False-Discovery-Rate (FDR) estimation
	static int			mFold;								// number of negative sequences as multiple of positive sequences
	static int			cvFold;								// number of cross-validation folds
	// further FDR options...

	static bool			verbose;							// verbose printouts, defaults to false
	static bool			saveInitBaMMs;						// Write initialized BaMM(s) to disk.
	static bool			saveBaMMs;							// Write optimized BaMM(s) to disk.

	static void         init( int nargs, char* args[] );
	static void         destruct();

	static char* 		String( const char *s );			// convert const char* to string, for GetOpt library
	static int			ipow( unsigned int base, int exp );	// power function for integers
	static int*			powA;								// sizes of alphabet to the power k

	static bool			setSlow;							// develop with the slow EM version

private:

	static int	        readArguments( int nargs, char* args[] );
	static void	        createDirectory( char* dir );
	static char*		baseName( char* path );
	static void	        printHelp();
};

// format cast for GetOpt function
inline char* Global::String( const char *s ){
	return strdup( s );
}

namespace GetOpt{
	template <> inline _Option::Result convert<char*>( const std::string& s, char*& d, std::ios::fmtflags ){
		_Option::Result ret = _Option::BadType;
		d = Global::String( s.c_str() );
		ret = _Option::OK;
		return ret;
	}
}

#endif /* GLOBAL_H_ */
