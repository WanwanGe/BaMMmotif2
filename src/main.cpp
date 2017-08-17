#include <iomanip>

#include "Global.h"
#include "BackgroundModel.h"
#include "MotifSet.h"
#include "EM.h"
#include "GibbsSampling.h"
#include "ScoreSeqSet.h"
#include "SeqGenerator.h"
#include "FDR.h"

int main( int nargs, char* args[] ){

	clock_t t0 = clock();

	fprintf( stderr, "\n" );
	fprintf( stderr, "======================================\n" );
	fprintf( stderr, "=      Welcome to use BaMM!motif     =\n" );
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
		bgModel = new BackgroundModel( Global::negSequenceSet->getSequences(),
										Global::bgModelOrder,
										Global::bgModelAlpha,
										Global::interpolateBG,
										Global::posSequenceBasename );
	} else {
		bgModel = new BackgroundModel( Global::bgModelFilename );
	}

	// always save background model
	bgModel->write( Global::outputDirectory, Global::posSequenceBasename );

	if( Global::verbose ){
		fprintf( stderr, "\n" );
		fprintf( stderr, "**************************\n" );
		fprintf( stderr, "*   Initial Motif Model  *\n" );
		fprintf( stderr, "**************************\n" );
	}

	MotifSet motif_set( Global::initialModelFilename, Global::addColumns.at(0), Global::addColumns.at(1),
					Global::initialModelTag );

	size_t motifNum = ( Global::num > motif_set.getN() ) ? motif_set.getN() : Global::num;

	if( Global::verbose ){
		fprintf( stderr, "\n" );
		fprintf( stderr, "********************\n" );
		fprintf( stderr, "*   BaMM training  *\n" );
		fprintf( stderr, "********************\n" );
	}

    for( size_t n = 0; n < motifNum; n++ ){
		// deep copy each motif in the motif set
		Motif* motif = new Motif( *motif_set.getMotifs()[n] );

		if( Global::saveInitialBaMMs ){
			// optional: save initial model
			motif->write( Global::outputDirectory, Global::posSequenceBasename, 0 );
		}

		// train the model with either EM or Gibbs sampling
		if( Global::EM ){
			EM model( motif, bgModel, Global::posSequenceSet->getSequences(), Global::q );
			// learn motifs by EM
			model.optimize();
			// write model parameters on the disc
			if( Global::saveBaMMs ){
				model.write( Global::outputDirectory, Global::posSequenceBasename, n+1, Global::ss );
			}

		} else if ( Global::CGS ){
			GibbsSampling model( motif, bgModel,  Global::posSequenceSet->getSequences(), Global::q );
			// learn motifs by collapsed Gibbs sampling
			model.optimize();
			// write model parameters on the disc
			if( Global::saveBaMMs ){
				model.write( Global::outputDirectory, Global::posSequenceBasename, n+1, Global::ss );
			}

		} else {

			std::cout << "Note: the model is not optimized!\n";

		}

		// write out the learned model
		motif->write( Global::outputDirectory, Global::posSequenceBasename, n+1 );

		if( Global::scoreSeqset ){
			// score the model on sequence set
			ScoreSeqSet seq_set( motif, bgModel, Global::posSequenceSet->getSequences() );
			seq_set.score();
			seq_set.write( Global::outputDirectory, Global::posSequenceBasename, n+1, Global::scoreCutoff, Global::ss );
		}

		if( Global::generatePseudoSet ){

			// optimize motifs by EM
			EM model( motif, bgModel, Global::posSequenceSet->getSequences(), Global::q );
			model.optimize();

			// generate artificial sequence set with the learned motif embedded
			SeqGenerator artificial_seq_set( Global::posSequenceSet->getSequences(), motif, Global::sOrder );

			artificial_seq_set.write( Global::outputDirectory, Global::posSequenceBasename, n+1,
							artificial_seq_set.arti_posset_motif_embedded( Global::mFold ) );

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
        std::vector<Sequence*> posseqs = Global::posSequenceSet->getSequences();
        std::vector<Sequence*> negseqs = Global::negSequenceSet->getSequences();
        /**
         * Generate negative sequence set for generating bg model and scoring
         */
        std::vector<Sequence*> B1set;
        // sample negative sequence set B1set based on s-mer frequencies
        // from positive training sequence set
        std::vector<std::unique_ptr<Sequence>> B1Seqs;
        SeqGenerator b1seqs( posseqs );
        B1Seqs = b1seqs.arti_negset( Global::mFold );
        // convert unique_ptr to regular pointer
        for( size_t n = 0; n < B1Seqs.size(); n++ ){
            B1set.push_back( B1Seqs[n].release() );
        }

        std::vector<Sequence*> B2set;
        // sample negative sequence set B2set based on s-mer frequencies
        // from the given sampled negative sequence set
        std::vector<std::unique_ptr<Sequence>> B2Seqs;
        SeqGenerator b2seqs( negseqs );
        B2Seqs = b2seqs.arti_negset( 1 );
        // convert unique_ptr to regular pointer
        for( size_t n = 0; n < B2Seqs.size(); n++ ){
            B2set.push_back( B2Seqs[n].release() );
        }

        std::vector<Sequence*> B3set = negseqs;

        std::vector<Sequence*> B1setPrime;
        // generate sequences from positive sequences with masked motif
        Motif* motif_opti = new Motif( *motif_set.getMotifs()[0] );
        SeqGenerator artificial_set( posseqs, motif_opti );
        EM model_opti( motif_opti, bgModel, posseqs, Global::q );
        // learn motifs by EM
        model_opti.optimize();
        std::vector<std::unique_ptr<Sequence>> B1SeqSetPrime;
        B1SeqSetPrime = artificial_set.arti_negset_motif_masked( model_opti.getR() );
        if( motif_opti ) delete motif_opti;
        // draw B1setPrime from the positive sequence set with masked motifs
        for( size_t n = 0; n < B1SeqSetPrime.size(); n++ ){
            B1setPrime.push_back( B1SeqSetPrime[n].release() );
        }

		// cross-validate the motif model
		for( size_t n = 0; n < motifNum; n++ ){
			Motif* motif = new Motif( *motif_set.getMotifs()[n] );
			FDR fdr( Global::posSequenceSet->getSequences(), B1set,
					Global::q, motif, bgModel, Global::cvFold );
			fdr.evaluateMotif();
			fdr.write( Global::outputDirectory, Global::posSequenceBasename, n+1 );
			if( motif )		delete motif;
		}

	}

	fprintf( stderr, "\n" );
	fprintf( stderr, "******************\n" );
	fprintf( stderr, "*   Statistics   *\n" );
	fprintf( stderr, "******************\n" );
	Global::printStat();

	fprintf( stdout, "\n------ Runtime: %.2f seconds (%0.2f minutes) -------\n",
			( ( float )( clock() - t0 ) ) / CLOCKS_PER_SEC,
			( ( float )( clock() - t0 ) ) / ( CLOCKS_PER_SEC * 60.0f ) );

	// free memory
	if( bgModel ) delete bgModel;

	Global::destruct();

	return 0;
}
