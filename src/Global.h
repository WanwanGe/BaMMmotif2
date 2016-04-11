/*
 * Global.h
 *
 *  Created on: Apr 1, 2016
 *      Author: wanwan
 */

#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <vector>
#include <iostream>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "SequenceSet.h"
#include "Alphabet.h"

class Global {

public:

	static char* 		outputDirectory;      	// output directory

	static char* 		posSequenceFilepath;	// filename of positive sequence fasta file
	static char* 		negSequenceFilepath;	// filename of negative sequence fasta file

	static char const*	alphabetString;			// defaults to ACGT but may later be extended to ACGTH(hydroxymethylcytosine) or similar
	static bool			revcomp;				// also search on reverse complement of sequences

	static SequenceSet* posSequenceSet;			// positive Sequence Set
	static SequenceSet* negSequenceSet;			// negative Sequence Set

	static char* 		intensityFilename;		// filename of intensity file (i.e. for HT-SELEX data)
	// further weighting options...

	// files to initialize model(s)
	static char* 		BaMMpatternFilename;	// filename of BaMMpattern file
	static char* 		bindingSitesFilename;	// filename of binding sites file
	static char* 		PWMFilename;			// filename of PWM file
	static char* 		BMMFilename;			// filename of Markov model (.bmm) file

	// model options
	static unsigned int modelOrder;				// model order
	static float** 		modelAlpha;				// initial alphas
	static std::vector<int> addColumns;			// add columns to the left and right of models used to initialize Markov models
	static bool			noLengthOptimization;	// disable length optimization

	// background model options
	static unsigned int bgModelOrder;			// background model order, defaults to 2
	static float** 		bgModelAlpha;			// background model alphas

	// EM options
	static unsigned int	maxEMIterations;		// maximum number of iterations
	static float 		epsilon;				// likelihood convergence parameter

	static bool			noAlphaOptimization;	// disable alpha optimization
	static bool			noQOptimization;		// disable q optimization

	// FDR options
	static bool			FDR;					// triggers False-Discovery-Rate (FDR) estimation
	static unsigned int mFold;					// number of negative sequences as multiple of positive sequences
	static unsigned int nFolds;					// number of cross-validation folds
	static std::vector< std::vector<int> > posFoldIndices;// sequence indices for each cross-validation fold
	static std::vector< std::vector<int> > negFoldIndices;// sequence indices for each cross-validation fold
	// further FDR options...

	static bool			verbose;				// verbose printouts

	static void init( int nargs, char* args[] ){
		readArguments_( nargs, args );
		createDirectory_( outputDirectory );
		Alphabet::init( alphabetString );
		// read in positive (and negative) sequence set

		// generate folds (fill posFoldIndices and negFoldIndices)
		generateFolds_( posSequenceSet->getN(), negSequenceSet->getN(), nFolds );
		// optional: read in sequence intensities (header and intensity columns?)
		if( intensityFilename != 0){
			// read in sequence intensity
		}
	}

	static void destruct(){
		Alphabet::destruct();
		// ...
	}

private:

	static int 	readArguments_( int nargs, char* args[] );
	static void printHelp_();
	static void createDirectory_( const char* dir );
	static void generateFolds_( int posCount, int negCount, int fold );
};

#endif /* GLOBAL_H_ */
