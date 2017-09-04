//
// Created by wanwan on 03.09.17.
//

#ifndef GFDR_H_
#define GFDR_H_

#include <string.h>	                            // e.g. strdup

#include "../init/Alphabet.h"
#include "../init/SequenceSet.h"
#include "../getopt_pp/getopt_pp.h"             // GetOpt function

class GFdr{

public:

    static char*		outputDirectory; 		// output directory

    // sequence set options
    static char*		posSequenceFilename;	// filename of positive sequence FASTA file
    static std::string	posSequenceBasename;	// basename of positive sequence FASTA file
    static SequenceSet*	posSequenceSet;			// positive sequence set
    static float		q;						// prior probability for a positive sequence to contain a motif
    static bool			ss;						// only search on single strand sequences
    static char*		negSequenceFilename;	// filename of negative sequence FASTA file
    static SequenceSet*	negSequenceSet;			// negative sequence set
    static char* 		alphabetType;			// provide alphabet type

    // initial model(s) options
    static char*		initialModelFilename;	// filename of initial model
    static std::string	initialModelTag;		// tag for initializing the model
    static size_t		num;					// number of init that are to be optimized
    static bool			mops;					// learn MOPS model
    static bool			zoops;					// learn ZOOPS model

    // model options
    static size_t		modelOrder;				// model order
    static std::vector<float> modelAlpha;		// initial alphas
    static float		modelBeta;				// alpha_k = beta x gamma^k for k > 0
    static float		modelGamma;
    static std::vector<size_t>	addColumns;		// add columns to the left and right of init used to initialize Markov init
    static bool			interpolateBG;			// calculate prior probabilities from lower-order probabilities
                                                // instead of background frequencies of mono-nucleotides

    // background model options
    static size_t		bgModelOrder;			// background model order, defaults to 2
    static std::vector<float> bgModelAlpha;		// background model alpha

    // refinement options
    static bool			EM;						// flag to trigger EM learning
    static bool			CGS;					// flag to trigger Collapsed Gibbs sampling

    // FDR options
    static size_t		mFold;					// number of negative sequences as multiple of positive sequences
	static size_t		cvFold;					// number of cross-validation (cv) folds
	static size_t		sOrder;					// k-mer order for sampling negative sequence set
    // other options
    static bool			savePRs;				// write the precision, recall, TP and FP
    static bool			savePvalues;			// write p-values for each log odds score from sequence set
    static bool			saveLogOdds;			// write the log odds of positive and negative sets to disk

    static void         init( int nargs, char* args[] );
    static void         destruct();

    static char*        String( const char *s ); // GetOpt library
private:

    static int	        readArguments( int nargs, char* args[] );
    static void	        printHelp();
};

inline char* GFdr::String( const char *s ){
	return strdup( s );
}
namespace GetOpt{
	template <> inline _Option::Result convert<char*>( const std::string& s, char*& d, std::ios::fmtflags ){
		_Option::Result ret = _Option::BadType;
		d = GFdr::String( s.c_str() );
		ret = _Option::OK;
		return ret;
	}
}
#endif //GFDR_H_
