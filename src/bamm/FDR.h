#ifndef FDR_H_
#define FDR_H_

#include "../shared/BackgroundModel.h"
#include "../shared/utils.h"
#include "Motif.h"
#include "EM.h"

class FDR {

public:

	FDR( Motif* motif );
//	FDR( const Motif& motif );
	~FDR();

	void evaluateMotif();
	void print();
	void write();

private:

	Motif*				motif_;				// motif learned on full SequenceSet

	std::vector<int>	testFold_;			// fold index for test set
	std::vector<int>	trainFolds_;		// fold index for training set

	float**				posS_;				// store the full set of log odds scores for positive set
	float**				negS_;				// store the full set of log odds scores for negative set

	std::vector<float>	posS_all_;			// log odds scores on positive test SequenceSets
	std::vector<float> 	negS_all_;			// log odds scores on negative test SequenceSets
	std::vector<float>	posS_max_;			// largest log odds scores on positive test SequenceSet
	std::vector<float>	negS_max_;			// largest log odds scores on negative test SequenceSet

	std::vector<float>	P_ZOOPS_;			// precision for ZOOPS model
	std::vector<float>	R_ZOOPS_;			// recall for ZOOPS model
	float*				TP_ZOOPS_;			// true positive values for ZOOPS model

	std::vector<float>	P_MOPS_;			// precision for MOPS model
	std::vector<float>	R_MOPS_;			// recall for MOPS model
	float*				FP_MOPS_;			// false positive values for MOPS model
	float*				TFP_MOPS_;			// true and false positive values for MOPS model

					// generate background sample sequence set
	SequenceSet*	sampleSequenceSet( EM* em );
	Sequence*		sampleSequence();

					// score Global::posSequenceSet using Global::foldIndices and folds
	void 	        scoreSequenceSet( Motif* motif, BackgroundModel* bg, std::vector<int> testFold );

					// score SequenceSet sequences
	void 		    scoreSequenceSet( Motif* motif, BackgroundModel* bg, SequenceSet* seqSet );

	void 		    calculatePR();

					// Quick sort algorithm in descending order
	void			quickSort( std::vector<float> arr, int left, int right );
};

inline void quickSort( std::vector<float> arr, int left, int right ) {

	int i = left, j = right;
	float tmp;
	float pivot = arr[( left + right ) / 2];

	/* partition */
	while( i <= j ){
		while( arr[i] - pivot > 0 )	i++;
		while( arr[j] - pivot < 0 )	j--;
		if( i <= j ){
			tmp = arr[i];
			arr[i] = arr[j];
			arr[j] = tmp;
			i++;
			j--;
		}
	}

	/* recursion */
	if( left < j )	quickSort( arr, left, j );
	if( i < right )	quickSort( arr, i, right );
}

#endif /* FDR_H_ */

