/*
 * FDR.h
 *
 *  Created on: Apr 1, 2016
 *      Author: wanwan
 */

#ifndef FDR_H_
#define FDR_H_

#include "BackgroundModel.h"
#include "Motif.h"

class FDR {

public:

	FDR( const Motif& motif ){
		// allocate memory for posLogOddsScore, negLogOddsScore, P, and R
	}
	~FDR();

	void evaluateMotif(){

		for( int fold = 0; fold < Global::nFolds; fold++ ){

			Motif* motif = new Motif( this.motif );

			// assign training and test folds
			std::vector<int> trainingFold;
			for( int f = 0; f < Global::nFolds; f++ ){
				if( f != fold ){
					trainingFold.push_back( f );
				}
			}
			std::vector<int> testFold{ fold };

			BackgroundModel bg;
			bg.init( trainingFold );

			EM em = new EM( *motif, bg, trainingFold );
			em.learnMotif();

			// score positive test sequences
			scoreSequenceSet( motif, bg, testFold, posS );

			// generate negative test sequences
			SequenceSet* negSequenceSet = sampleSequenceSet();
			// score negative test sequences
			scoreSequenceSet( motif, bg, negSequenceSet, negS );
		}
		calculatePR();
	}

	void print();
	void write();

private:

	const Motif&	motif_;				// motif learned on full SequenceSet

	float** 		posS_[n][i];		// log-odds scores on positive test SequenceSets
	float** 		negS_[nm][i];		// log-odds scores on negative test SequenceSets

	float* 			P_;					// precision
	float* 			R_;					// recall

	SequenceSet*	sampleSequenceSet();
	Sequence*		sampleSequence();

					// score Global::posSequenceSet using Global::foldIndices and folds
	void 			scoreSequenceSet( Motif* motif, BackgroundModel* bg, std::vector<int> folds, float** S );
					// score SequenceSet sequences
	void 			scoreSequenceSet( Motif* motif, BackgroundModel* bg, SequenceSet* sequences, float** S );
					// score Sequence sequence
	float* 			scoreSequence( Motif* motif, BackgroundModel* bg, Sequence* sequence );

	void 			calculatePR();
};



#endif /* FDR_H_ */
