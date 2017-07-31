#include <iomanip>
#include <time.h>		// time(), clock_t, clock, CLOCKS_PER_SEC
#include <stdio.h>

#include "Global.h"
#include "BackgroundModel.h"
#include "utils.h"
#include "MotifSet.h"
#include "ModelLearning.h"
#include "ScoreSeqSet.h"
#include "SeqGenerator.h"
#include "FDR.h"

int main( int nargs, char* args[] ){

	clock_t t0 = clock();

	fprintf( stderr, "\n" );
	fprintf( stderr, "======================================\n" );
	fprintf( stderr, "=      Welcome to use BaMMmotif      =\n" );
	fprintf( stderr, "=                   Version 2.0      =\n" );
	fprintf( stderr, "=              by Soeding Group      =\n" );
	fprintf( stderr, "=  http://www.mpibpc.mpg.de/soeding  =\n" );
	fprintf( stderr, "======================================\n" );

	// seed random number
	srand( 42 );
	Global::rngx.seed( 42 );

	// initialization
	Global::init( nargs, args );

	if( Global::verbose ){
		fprintf( stderr, "\n" );
		fprintf( stderr, "************************\n" );
		fprintf( stderr, "*   Background Model   *\n" );
		fprintf( stderr, "************************\n" );
	}
	BackgroundModel* bgModel;
	if( !Global::bgModelGiven ){
		bgModel = new BackgroundModel( Global::bgSequenceSet->getSequences(),
										Global::bgModelOrder,
										Global::bgModelAlpha,
										Global::interpolateBG,
										Global::bgSequenceSet->getSequenceFilepath());
	} else {
		bgModel = new BackgroundModel( Global::bgModelFilename );
	}

	if( Global::saveBgModel ){
		bgModel->write( Global::outputDirectory );
	}

	if( Global::verbose ){
		fprintf( stderr, "\n" );
		fprintf( stderr, "**************************\n" );
		fprintf( stderr, "*   Initial Motif Model  *\n" );
		fprintf( stderr, "**************************\n" );
	}

	MotifSet motifs( Global::initialModelFilename,
					Global::addColumns.at(0),
					Global::addColumns.at(1),
					Global::initialModelTag );

	size_t motifNum = ( Global::num > motifs.getN() ) ?
						motifs.getN() : Global::num;

	if( Global::verbose ){
		fprintf( stderr, "\n" );
		fprintf( stderr, "********************\n" );
		fprintf( stderr, "*   BaMM training  *\n" );
		fprintf( stderr, "********************\n" );
	}

	for( size_t n = 0; n < motifNum; n++ ){
		// deep copy each motif in the motif set
		Motif* motif = new Motif( *motifs.getMotifs()[n] );

		// train the model with either EM or Gibbs sampling
		ModelLearning model( motif,
							bgModel,
							Global::posSequenceSet->getSequences(),
							Global::q );

		if( Global::saveInitialBaMMs ){
			// optional: save initial model
			motif->write( Global::outputDirectory,
							Global::posSequenceBasename, 0 );
		}

		if( Global::EM ){
			// learn motifs by EM
			model.EM();

		} else if ( Global::CGS ){
			// learn motifs by collapsed Gibbs sampling
			model.GibbsSampling();

		} else {

			std::cout << "Note: the model is not optimized!\n";

		}

		// write model parameters on the disc
		if( Global::saveBaMMs ){
			model.write( Global::outputDirectory,
							Global::posSequenceBasename, n+1 );
		}

		// write out the learned model
		// motif->write( n+1 );
		model.getMotif()->write( Global::outputDirectory,
									Global::posSequenceBasename, n+1 );

		if( Global::scoreSeqset ){
			// score the model on sequence set
			ScoreSeqSet seqset( motif, bgModel,
								Global::posSequenceSet->getSequences() );
			seqset.score();
			seqset.write( Global::outputDirectory, Global::posSequenceBasename,
					n+1, Global::scoreCutoff, Global::ss );
		}

		if( Global::generatePseudoSet ){

			// optimize motifs by EM
			model.EM();

			// generate pseudo positive sequence set based on the learned motif
			SeqGenerator seqset( Global::posSequenceSet->getSequences(),
									model.getMotif(),
									Global::sOrder );

			seqset.generate_seqset_with_embedded_motif( Global::mFold );

			seqset.write_seqset_with_embedded_motif( Global::outputDirectory,
									Global::posSequenceBasename, n+1 );

		}

		delete motif;
	}

	// evaluate motifs
	if( Global::FDR ){
		if( Global::verbose ){
			fprintf( stderr, "\n" );
			fprintf( stderr, "*********************\n" );
			fprintf( stderr, "*  BaMM validation  *\n" );
			fprintf( stderr, "*********************\n" );
		}
		// generate positive and negative sequence set for FDR
		std::vector<Sequence*> posSet = Global::posSequenceSet->getSequences();
		std::vector<Sequence*> negSet;
		if( !Global::negSeqGiven ){
			// sample negative sequence set based on s-mer frequencies
			// from positive sequence set
			std::vector<std::unique_ptr<Sequence>> negSeqs;
			SeqGenerator seqs( posSet );
			// from given negative sequence set
			//SeqGenerator seqs( Global::negSequenceSet->getSequences() );
			negSeqs = seqs.generate_negative_seqset( Global::mFold );
			// convert unique_ptr to regular pointer
			for( size_t n = 0; n < negSeqs.size(); n++ ){
				negSet.push_back( negSeqs[n].release() );
			}

		} else {
			negSet = Global::negSequenceSet->getSequences();
		}

		for( size_t n = 0; n < motifNum; n++ ){
			Motif* motif = new Motif( *motifs.getMotifs()[n] );
			FDR fdr( posSet, negSet, Global::q,
					motif, bgModel, Global::cvFold );
			fdr.evaluateMotif();
			fdr.write( Global::outputDirectory,
					Global::posSequenceBasename, n+1 );
			if( motif ) delete motif;
		}
	}

	fprintf( stderr, "\n" );
	fprintf( stderr, "******************\n" );
	fprintf( stderr, "*   Statistics   *\n" );
	fprintf( stderr, "******************\n" );
	Global::printStat();

	fprintf( stdout, "\n-------------- Runtime: %.2f seconds (%0.2f minutes) --------------\n",
			( ( float )( clock() - t0 ) ) / CLOCKS_PER_SEC,
			( ( float )( clock() - t0 ) ) / ( CLOCKS_PER_SEC * 60.0f ) );

	// free memory
	if( bgModel ) delete bgModel;
	Global::destruct();

	return 0;
}
