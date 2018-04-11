#include <iomanip>

#include "Global.h"
#include "EM.h"
#include "GibbsSampling.h"
#include "../evaluation/FDR.h"

int main( int nargs, char* args[] ){

	clock_t t0 = clock();

    std::cout << std::endl
              << "======================================" << std::endl
              << "=      Welcome to use BaMM!motif     =" << std::endl
              << "=                   Version 2.0      =" << std::endl
              << "=              by Soeding Group      =" << std::endl
              << "=  http://www.mpibpc.mpg.de/soeding  =" << std::endl
              << "======================================" << std::endl;

	// seed random number
	srand( 42 );
	Global::rngx.seed( 42 );

	// initialization
	Global::init( nargs, args );

	if( Global::verbose ){
		std::cout << std::endl
                  << "************************" << std::endl
                  << "*   Background Model   *" << std::endl
                  << "************************" << std::endl;
	}

	BackgroundModel* bgModel;
	if( !Global::bgModelGiven ){
		bgModel = new BackgroundModel( Global::posSequenceSet->getSequences(),
                                       Global::bgModelOrder,
                                       Global::bgModelAlpha,
                                       Global::interpolateBG,
                                       Global::outputFileBasename );

    } else {
		bgModel = new BackgroundModel( Global::bgModelFilename );

	}
    // always save the background model
    bgModel->write( Global::outputDirectory, Global::outputFileBasename );

	if( Global::verbose ){
        std::cout << std::endl
                  << "***************************" << std::endl
                  << "*   Initial Motif Model   *" << std::endl
                  << "***************************" << std::endl;
	}

	MotifSet motif_set( Global::initialModelFilename,
                        Global::addColumns.at(0),
                        Global::addColumns.at(1),
                        Global::initialModelTag,
                        Global::posSequenceSet,
                        Global::posSequenceSet->getBaseFrequencies(),
                        Global::modelOrder,
                        Global::modelAlpha,
                        Global::maxPWM);

	if( Global::verbose ){
        std::cout << std::endl
                  << "*********************" << std::endl
                  << "*   BaMM Training   *" << std::endl
                  << "*********************" << std::endl;
	}

    // sample negative sequence set B1set based on s-mer frequencies
    // from positive training sequence set
    std::vector<Sequence*>  negset;
    std::vector<std::unique_ptr<Sequence>> negSeqs;
    size_t minSeqN = 5000;
    SeqGenerator negseq( Global::posSequenceSet->getSequences() );
    if( Global::posSequenceSet->getSequences().size() >= minSeqN ){
        negSeqs = negseq.sample_bgseqset_by_fold( Global::mFold );
    } else {
        negSeqs = negseq.sample_bgseqset_by_num( minSeqN, Global::posSequenceSet->getMaxL());
    }
    // convert unique_ptr to regular pointer
    for( size_t n = 0; n < negSeqs.size(); n++ ) {
        negset.push_back( negSeqs[n].release() );
        negSeqs[n].get_deleter();
    }

#pragma omp parallel for
    for( size_t n = 0; n < motif_set.getN(); n++ ){
		// deep copy each motif in the motif set
		Motif* motif = new Motif( *motif_set.getMotifs()[n] );

        // make sure the motif length does not exceed the sequence length
        size_t minPosL = ( Global::ss ) ? Global::posSequenceSet->getMinL() : Global::posSequenceSet->getMinL() * 2;
        size_t minNegL = ( Global::ss ) ? Global::negSequenceSet->getMinL() : Global::negSequenceSet->getMinL() * 2;
        assert( motif->getW() <= minPosL );
        assert( motif->getW() <= minNegL );

		if( Global::saveInitialBaMMs ){
			// optional: save initial model
			motif->write( Global::outputDirectory,
                          Global::outputFileBasename + "_init_motif_" + std::to_string( n+1 ) );
		}

		// optimize the model with either EM or Gibbs sampling
		if( Global::EM ){
			EM model( motif, bgModel, Global::posSequenceSet->getSequences(),
                      Global::q, Global::optimizeQ, Global::verbose, Global::f );
			// learn motifs by EM
			if( !Global::advanceEM ) {
                model.optimize();
            } else {
                model.mask();
            }

            // write model parameters on the disc
			if( Global::saveBaMMs ){
				model.write( Global::outputDirectory,
                             Global::outputFileBasename + "_motif_" + std::to_string( n+1 ),
                             Global::ss );
			}

            // print out the optimized q for checking:
            std::cout << "optimized q = " << model.getQ() << std::endl;

		} else if ( Global::CGS ){
			GibbsSampling model( motif, bgModel, Global::posSequenceSet->getSequences(),
                                 Global::q, !Global::noQSampling, Global::verbose );
			// learn motifs by collapsed Gibbs sampling
			model.optimize();
			// write model parameters on the disc
			if( Global::saveBaMMs ){
				model.write( Global::outputDirectory,
                             Global::outputFileBasename + "_motif_" + std::to_string( n+1 ),
                             Global::ss );
			}

            // print out the optimized q for checking:
            std::cout << "optimized q = " << model.getQ() << std::endl;

		} else {

			std::cout << "Note: the model is not optimized!\n";
		}
        // write out the learned model
        motif->write( Global::outputDirectory,
                      Global::outputFileBasename + "_motif_" + std::to_string( n+1 ) );

        if( Global::scoreSeqset ){
            // score the model on sequence set
            if( Global::verbose ) {
                std::cout << std::endl
                          << "*************************" << std::endl
                          << "*    Score Sequences    *" << std::endl
                          << "*************************" << std::endl
                          << std::endl;
            }

            // Define bg model depending on motif input, and learning
            // use bgModel generated from input sequences when prediction is turned on
            BackgroundModel* bg = bgModel;
            // if no optimization is applied, get bgModel from the input
            if( !Global::EM and !Global::CGS ) {
                // use provided bgModelFile if initialized with bamm format
                if( Global::initialModelTag == "BaMM" ) {
                    if( Global::bgModelFilename == NULL ) {
                        std::cout << "No background Model file provided for initial search motif!\n";
                        exit( 1 );
                    }
                    bg = new BackgroundModel( Global::bgModelFilename );
                } else if( Global::initialModelTag == "PWM" ){
                    // this means that also the global motif order needs to be adjusted;
                    Global::modelOrder = 0;
                }
            }

            ScoreSeqSet scoreNegSet( motif, bgModel, negset );
            scoreNegSet.calcLogOdds();
            std::vector<std::vector<float>> negAllScores = scoreNegSet.getMopsScores();
            std::vector<float> negScores;
            for( size_t n = 0; n < negset.size(); n++ ){
                negScores.insert( std::end( negScores ),
                                  std::begin( negAllScores[n] ),
                                  std::end( negAllScores[n] ) );
            }

            // calculate p-values based on positive and negative scores
            ScoreSeqSet scorePosSet( motif, bg, Global::posSequenceSet->getSequences() );
            scorePosSet.calcLogOdds();
            if( Global::saveLogOdds ){
                scorePosSet.writeLogOdds(Global::outputDirectory,
                                         Global::outputFileBasename + "_motif_" + std::to_string( n+1 ),
                                         Global::ss );
            }
            std::vector<std::vector<float>> posScores = scorePosSet.getMopsScores();
            scorePosSet.calcPvalues( posScores, negScores );

            scorePosSet.write( Global::outputDirectory,
                               Global::outputFileBasename + "_motif_" + std::to_string( n+1 ),
                               Global::pvalCutoff,
                               Global::ss );

        }
        if( motif )		delete motif;
	}

    // evaluate motifs
	if( Global::FDR ){
		if( Global::verbose ){
            std::cout << std::endl
                      << "***********************" << std::endl
                      << "*   BaMM validation   *" << std::endl
                      << "***********************" << std::endl;
		}

        /**
         * cross-validate the motif model
         */
#pragma omp parallel for
        for( size_t n = 0; n < motif_set.getN(); n++ ){
			Motif* motif = new Motif( *motif_set.getMotifs()[n] );
			FDR fdr( Global::posSequenceSet->getSequences(), negset,
                     Global::q, motif, bgModel, Global::cvFold,
                     Global::mops, Global::zoops,
                     Global::savePRs, Global::savePvalues, Global::saveLogOdds );
			fdr.evaluateMotif( Global::EM, Global::CGS, Global::optimizeQ, Global::advanceEM, Global::f );
			fdr.write( Global::outputDirectory,
                       Global::outputFileBasename + "_motif_" + std::to_string( n+1 ) );
			if( motif )		delete motif;
		}

    }

    std::cout << std::endl
              << "******************" << std::endl
              << "*   Statistics   *" << std::endl
              << "******************" << std::endl;
	Global::printStat();

	fprintf( stdout, "\n------ Runtime: %.2f seconds (%0.2f minutes) -------\n",
			( ( float )( clock() - t0 ) ) / CLOCKS_PER_SEC,
			( ( float )( clock() - t0 ) ) / ( CLOCKS_PER_SEC * 60.0f ) );

	// free memory
	if( bgModel ) delete bgModel;

	Global::destruct();

	return 0;
}
