#include "FDR.h"

#include <float.h>		// -FLT_MAX

FDR::FDR( Motif* motif ){

	motif_ = motif;

	for( int k = 0; k < std::max( Global::modelOrder+2 , Global::bgModelOrder+2 ); k++ ){
		Y_.push_back( ipow( Alphabet::getSize(), k ) );
	}

	testsetV_ = ( float** )calloc( Global::sOrder+1, sizeof( float* ) );
	testsetN_ = ( int** )calloc( Global::sOrder+1, sizeof( int* ) );
	for( int k = 0; k < Global::sOrder+1; k++ ){
		testsetV_[k] = ( float* )calloc( Y_[k+1], sizeof( float ) );
		testsetN_[k] = ( int* )calloc( Y_[k+1], sizeof( int ) );
	}

	occ_frac_ = 0.0f;

	occ_mult_ = 0.0f;

}

FDR::~FDR(){

	for( int k = 0; k < Global::sOrder+1; k++ ){
		free( testsetV_[k] );
		free( testsetN_[k] );
	}
	free( testsetV_ );
	free( testsetN_ );
}

void FDR::evaluateMotif(){

	std::vector<Sequence*> posSeqs = Global::posSequenceSet->getSequences();
	std::vector<Sequence*> negSeqs = Global::negSequenceSet->getSequences();

	std::vector<std::vector<float>> scores;

	for( int fold = 0; fold < Global::cvFold; fold++ ){

		fprintf(stderr, " ________________________________\n"
						"|                                |\n"
						"|  Cross validation for fold-%d   |\n"
						"|________________________________|\n\n", fold+1 );

		Motif* motif = new Motif( *motif_ );			// deep copy the initial motif

		// assign training folds
		std::vector<int> trainsetFolds;
		trainsetFolds.resize( Global::cvFold - 1 );
		for( int f = 0; f < Global::cvFold; f++ ){
			if( f != fold ){
				trainsetFolds.push_back( f );
			}
		}

		// draw sequences for each test set
		std::vector<Sequence*> testSet;
		for( size_t i = 0; i < Global::posFoldIndices[fold].size(); i++ ){
			testSet.push_back( posSeqs[Global::posFoldIndices[fold][i]] );
		}

		// obtain background model for each training set
		BackgroundModel* bgModel = new BackgroundModel( *Global::negSequenceSet,
													Global::bgModelOrder,
													Global::bgModelAlpha,
													Global::interpolateBG,
													Global::negFoldIndices,
													trainsetFolds );

		// learn motif from each training set
		ModelLearning model( motif, bgModel, trainsetFolds );
		if( Global::EM ){
			model.EM();
		} else if ( Global::CGS ){
			model.GibbsSampling();
		}

		// score positive test sequences on optimized motif
		scores = scoreSequenceSet( motif, bgModel, testSet );
		posScoreAll_.insert( std::end( posScoreAll_ ), std::begin( scores[0] ), std::end( scores[0] ) );
		posScoreMax_.insert( std::end( posScoreMax_ ), std::begin( scores[1] ), std::end( scores[1] ) );


		if( !Global::negSeqGiven ){
			std::vector<std::unique_ptr<Sequence>> negSet;
			// generate negative sequence set
			negSet = sampleSequenceSet( testSet );
			// score negative sequence set
			scores = scoreSequenceSet( motif, bgModel, negSet );
		} else {
			std::vector<Sequence*> negSet;
			for( size_t i = 0; i < Global::negFoldIndices[fold].size(); i++ ){
				negSet.push_back( negSeqs[Global::negFoldIndices[fold][i]] );
			}
			// score negative sequence set
			scores = scoreSequenceSet( motif, bgModel, negSet );
		}

		negScoreAll_.insert( std::end( negScoreAll_ ), std::begin( scores[0] ), std::end( scores[0] ) );
		negScoreMax_.insert( std::end( negScoreMax_ ), std::begin( scores[1] ), std::end( scores[1] ) );

		delete motif;
		delete bgModel;
	}


	fprintf(stderr, " __________________________________\n"
					"|                                  |\n"
					"|  calculate precision and recall  |\n"
					"|__________________________________|\n\n" );
	calculatePR();


	fprintf(stderr, " ______________________\n"
					"|                      |\n"
					"|  calculate P-values  |\n"
					"|______________________|\n\n" );
	calculatePvalues();
}

// score sequences for the positive sequence set
std::vector<std::vector<float>> FDR::scoreSequenceSet( Motif* motif, BackgroundModel* bg, const std::vector<std::unique_ptr<Sequence>> & seqSet ){

	std::vector<std::vector<float>> scores( 2 );			// scores[0]: store the log odds scores at all positions of each sequence
															// scores[1]: store maximal log odds scores of each sequence
	int K = Global::modelOrder;
	int K_bg = Global::bgModelOrder;
	int W = motif->getW();
	float maxScore;											// maximal logOddsScore over all positions for each sequence

	for( size_t n = 0; n < seqSet.size(); n++ ){

		int LW1 = seqSet[n]->getL() - W + 1;

		maxScore = -FLT_MAX;

		std::vector<float> logOdds( LW1 );					// calculate log odds scores for all the positions

		for( int i = 0; i < LW1; i++ ){

			logOdds[i] = 0.0f;

			for( int j = 0; j < W; j++ ){

				int y = seqSet[n]->extractKmer( i+j, std::min(i+j, K ) );

				int y_bg = y % Y_[K_bg+1];

				if( y >= 0 ){
					logOdds[i] += ( logf( motif->getV()[K][y][j] ) - logf( bg->getV()[std::min( K, K_bg )][y_bg] ) );
				}
			}

			// take all the log odds scores for MOPS model:
			scores[0].push_back( logOdds[i] );

			// take the largest log odds score for ZOOPS model:
			if( logOdds[i] > maxScore ){
				maxScore = logOdds[i];
			}
		}

		scores[1].push_back( maxScore );

	}
	return scores;
}

// score sequences for the negative sequence set
std::vector<std::vector<float>> FDR::scoreSequenceSet( Motif* motif, BackgroundModel* bg, std::vector<Sequence*> seqSet ){

	std::vector<std::vector<float>> scores( 2 );			// scores[0]: store the log odds scores at all positions of each sequence
															// scores[1]: store maximal log odds scores of each sequence
	int K = Global::modelOrder;
	int K_bg = Global::bgModelOrder;
	int W = motif->getW();
	float maxScore;											// maximal logOddsScore over all positions for each sequence

	for( size_t n = 0; n < seqSet.size(); n++ ){

		int LW1 = seqSet[n]->getL() - W + 1;

		maxScore = -FLT_MAX;

		std::vector<float> logOdds( LW1 );					// calculate log odds scores for all the positions

		for( int i = 0; i < LW1; i++ ){

			logOdds[i] = 0.0f;

			for( int j = 0; j < W; j++ ){

				int y = seqSet[n]->extractKmer( i+j, std::min(i+j, K ) );

				int y_bg = y % Y_[K_bg+1];

				if( y >= 0 ){
					logOdds[i] += ( logf( motif->getV()[K][y][j] ) - logf( bg->getV()[std::min( K, K_bg )][y_bg] ) );
				}
			}

			// take all the log odds scores for MOPS model:
			scores[0].push_back( logOdds[i] );

			// take the largest log odds score for ZOOPS model:
			if( logOdds[i] > maxScore ){
				maxScore = logOdds[i];
			}
		}

		scores[1].push_back( maxScore );

	}
	return scores;
}

// generate negative sequences based on each test set
std::vector<std::unique_ptr<Sequence>> FDR::sampleSequenceSet( std::vector<Sequence*> seqs ){

	std::vector<std::unique_ptr<Sequence>> sampleSet;

	calcKmerFreq( seqs );

	for( size_t i = 0; i < seqs.size(); i++ ){
		int L = seqs[i]->getL();
		for( int n = 0; n < Global::mFold; n++ ){
			sampleSet.push_back( sampleSequence( L, testsetV_ ) );
		}
	}
	return sampleSet;
}

// generate sample sequence based on k-mer frequencies from positive set
std::unique_ptr<Sequence> FDR::sampleSequence( int L, float** v ){

	uint8_t* sequence = ( uint8_t* )calloc( L, sizeof( uint8_t ) );
	std::string header = "sample sequence";

	// get a random number for the first nucleotide
	double random = ( double )rand() / ( double )RAND_MAX;
	double f = 0.0f;
	for( uint8_t a = 1; a <= Y_[1]; a++ ){
		f += v[0][a-1];
		if( random <= f ){
			sequence[0] = a;
			break;
		}
		if( sequence[0] == 0 )	sequence[0] = a;		// Trick: this is to solve the numerical problem
	}

	for( int i = 1; i < L; i++ ){
		random = ( double )rand() / ( double )RAND_MAX;	// get another random double number
		// calculate y of K-mer
		int yk = 0;
		for( int k = ( i < Global::sOrder ) ? i : Global::sOrder; k > 0; k-- ){
			yk += ( sequence[i-k] - 1 ) * Y_[k];
		}

		// assign a nucleotide based on k-mer frequency
		f = 0.0f;
		for( uint8_t a = 1; a <= Y_[1]; a++ ){
			f += v[std::min( i, Global::sOrder )][yk+a-1];
			if( random <= f ){
				sequence[i] = a;
				break;
			}
			if( sequence[i] == 0 )	sequence[i] = a;	// Trick: this is to solve the numerical problem
		}
	}

	std::unique_ptr<Sequence> seq( new Sequence( sequence, L, header, Y_, Global::revcomp ) );

	free( sequence );

	return seq;

}

void FDR::calculatePR(){

	int M;
	int posN = Global::posSequenceSet->getN();
	int negN;
	if( Global::negSeqGiven ){
		negN = Global::negSequenceSet->getN();
		M = negN / posN;
	} else {
		M = Global::mFold;
		negN = posN * M;

	}
	// for MOPS model:
	// Sort log odds scores from large to small
	std::sort( posScoreAll_.begin(), posScoreAll_.end(), std::greater<float>() );
	std::sort( negScoreAll_.begin(), negScoreAll_.end(), std::greater<float>() );

	// Rank and score these log odds score values
	size_t idx_posAll = 0;
	size_t idx_negAll = 0;								// index for arrays storing the complete log odds scores
	float E_TP_MOPS = 0.0f;								// maximal distance between TFP and FP = expectation value of TP
	size_t idx_max = posN+negN;							// index when the distance between TFP and FP reaches maximum
														// otherwise, set total number as initial cutoff

	for( size_t i = 0; i < posScoreAll_.size() + negScoreAll_.size(); i++ ){
		if( posScoreAll_[idx_posAll] >= negScoreAll_[idx_negAll] || /* idx_posAll == 0 || */
				idx_negAll == posScoreAll_.size() + negScoreAll_.size()-1 ){
			idx_posAll++;
		} else {
			idx_negAll++;
		}

		MOPS_TP_.push_back( ( float )idx_posAll - ( float )idx_negAll / ( float )M );
		MOPS_FP_.push_back( ( float )idx_negAll / ( float )M );

		if( E_TP_MOPS == MOPS_TP_[i] ){
			idx_max = i;
//			break;										// stop when the distance between TFP and FP reaches maximum
		}
		if( E_TP_MOPS < MOPS_TP_[i] ){
			E_TP_MOPS = MOPS_TP_[i];
		}
	}

	for( size_t i = 0; i < idx_max; i++ ){
		MOPS_FDR_.push_back( MOPS_FP_[i] / ( MOPS_TP_[i] + MOPS_FP_[i] ) );
		MOPS_Rec_.push_back( MOPS_TP_[i] / E_TP_MOPS );
	}

	// the number of motif occurrences per sequence
	occ_mult_ = E_TP_MOPS / ( float )posN;

	// for ZOOPS model:
	// Sort log odds scores from large to small
	std::sort( posScoreMax_.begin(), posScoreMax_.end(), std::greater<float>() );
	std::sort( negScoreMax_.begin(), negScoreMax_.end(), std::greater<float>() );

	// Rank and score these log odds score values
	int idx_posMax = 0;
	int idx_negMax = 0;
	int min_idx_pos = 0;
	int posN_est = static_cast<int>( Global::q * ( float )posN );

	for( int i = 0; i < posN + negN; i++ ){
		if( posScoreMax_[idx_posMax] >= negScoreMax_[idx_negMax] ||
				/* idx_posMax == 0 || */ idx_negMax == posN+negN-1 ){
			idx_posMax++;
		} else {
			idx_negMax++;
		}

		ZOOPS_TP_.push_back( ( float )idx_posMax );
		ZOOPS_FP_.push_back( ( float )idx_negMax / ( float )M );

		if( idx_posMax == posN_est ){
			min_idx_pos = i + 1;
		}

		ZOOPS_FDR_.push_back( 1 - ( float )idx_posMax / ( float )(i+1) );
		ZOOPS_Rec_.push_back( ( float )idx_posMax / ( float )posN );

	}

	// the fraction of motif occurrence
	occ_frac_ = 1 - ZOOPS_FP_[min_idx_pos] / ( float )posN;

}

void FDR::calculatePvalues(){

	/**
	 *  calculate p-values for the log odds scores from positive sequence set
	 * 	later used for evaluating models using fdrtool R package
	 */

	int M;
	if( Global::negSeqGiven ){
		M = Global::negSequenceSet->getN() / Global::posSequenceSet->getN();
	} else {
		M = Global::mFold;
	}

	// for MOPS model:
	// Sort log odds scores from large to small
	std::sort( posScoreAll_.begin(), posScoreAll_.end(), std::greater<float>() );
	std::sort( negScoreAll_.begin(), negScoreAll_.end(), std::greater<float>() );

	// Rank and score these log odds score values
	size_t idx_posAll = 0;
	size_t idx_negAll = 0;
	size_t len_all = posScoreAll_.size() + negScoreAll_.size();

	for( size_t i = 0; i < len_all; i++ ){
		if( posScoreAll_[idx_posAll] >= negScoreAll_[idx_negAll]
		  || idx_posAll == 0 || idx_negAll == len_all - 1){
			idx_posAll++;
			MOPS_Pvalue_.push_back( ( ( float )idx_negAll + 0.5f )
					/ ( float )( negScoreAll_.size()  + 1 ) );
		} else {
			idx_negAll++;
		}

	}

	// for ZOOPS model:
	// Sort log odds scores from large to small
	std::sort( posScoreMax_.begin(), posScoreMax_.end(), std::greater<float>() );
	std::sort( negScoreMax_.begin(), negScoreMax_.end(), std::greater<float>() );

	// Rank and score these log odds score values
	size_t idx_posMax = 0;
	size_t idx_negMax = 0;
	size_t len_max = posScoreMax_.size() + negScoreMax_.size();
	for( size_t i = 0; i < len_max; i++ ){
		if( posScoreMax_[idx_posMax] >= negScoreMax_[idx_negMax]
		    || idx_posMax == 0 || idx_negMax == len_max - 1 ){
			idx_posMax++;
			ZOOPS_Pvalue_.push_back( ( ( float )idx_negMax + 0.5f )
					/ ( float )( M * posScoreMax_.size() + 1 ) );
		} else {
			idx_negMax++;
		}
	}

}

void FDR::calcKmerFreq( std::vector<Sequence*> seqs ){

	// reset counts for k-mers
	for( int k = 0; k < Global::sOrder+1; k++ ){
		for( int y = 0; y < Y_[k+1]; y++ ){
			testsetN_[k][y] = 0;
		}
	}

	// count k-mers
	for( size_t i = 0; i < seqs.size(); i++ ){
		int L = seqs[i]->getL();
		for( int k = 0; k < Global::sOrder+1; k++ ){
			// loop over sequence positions
			for( int j = k; j < L; j++ ){
				// extract (k+1)-mer
				int y = seqs[i]->extractKmer( j, k );
				// skip non-defined alphabet letters
				if( y >= 0 ){
					// count (k+1)mer
					testsetN_[k][y]++;
				}
			}
		}
	}

	// calculate frequencies
	int normFactor = 0;
	for( int y = 0; y < Y_[1]; y++ )	normFactor += testsetN_[0][y];
	for( int y = 0; y < Y_[1]; y++ )	testsetV_[0][y] = ( float )testsetN_[0][y] / ( float )normFactor;
	for( int k = 1; k < Global::sOrder+1; k++ ){
		for( int y = 0; y < Y_[k+1]; y++ ){
			int yk = y / Y_[1];
			testsetV_[k][y] = ( float )testsetN_[k][y] / ( float )testsetN_[k-1][yk];
		}
	}

/*	// correct k-mer frequencies
//	float eta0 = 0.9945f;
	float eta0 = 0.1f;
	int W = motif_->getW();
	int K = Global::modelOrder;
	for( int k = 0; k < K; k++ ){
		for( int y = 0; y < Y_[k+1]; y++ ){
			float corr = 0.0f;
			for( int j = K; j < W; j++){
				corr += motif_->getV()[k][y][j];
			}
			testsetV_[k][y] = ( 1 - ( 1 - eta0 ) * ( float )( W - K ) ) * testsetV_[k][y] + ( 1 - eta0 ) * corr;
		}
	}
	*/
}

void FDR::print(){

}

void FDR::writePR( int n ){

	/**
	 * save FDR parameters in three flat files:
	 * (1) posSequenceBasename.zoops.stats: 	TP, FP, precision and recall for ZOOPS model
	 * (2) posSequenceBasename.mops.stats:		TP, FP, precision and recall for MOPS model
	 * (3) posSequenceBasename.occ:				fraction occurrence and motif occurrences per sequence
	 */

	std::string opath = std::string( Global::outputDirectory ) + '/'
			+ Global::posSequenceBasename + "_motif_" + std::to_string( n+1 );

	std::string opath_zoops_stats = opath + ".zoops.stats";
	std::string opath_mops_stats = opath + ".mops.stats";
	std::string opath_occ = opath + ".occ";

	std::ofstream ofile_zoops( opath_zoops_stats );
	std::ofstream ofile_mops( opath_mops_stats );
	std::ofstream ofile_occ( opath_occ );
	size_t i;

	// for ZOOPS model:
	ofile_zoops << "TP" << '\t' << "FP" << '\t'
				<< "Precision" << '\t'
				<< "Recall" << '\t' << std::endl;

	for( i = 0; i < ZOOPS_FDR_.size(); i++ ){
		ofile_zoops << ZOOPS_TP_[i]  << '\t'
					<< ZOOPS_FP_[i]  << '\t'
					<< ZOOPS_FDR_[i] << '\t'
					<< ZOOPS_Rec_[i] << '\t' << std::endl;
	}

	// for MOPS model:
	ofile_mops  << "TP" << '\t' << "FP" << '\t'
				<< "Precision" << '\t'
				<< "Recall" << '\t' << std::endl;

	for( i = 0; i < MOPS_FDR_.size(); i++ ){
		ofile_mops  << MOPS_TP_[i]  << '\t'
					<< MOPS_FP_[i]  << '\t'
					<< MOPS_FDR_[i] << '\t'
					<< MOPS_Rec_[i] << '\t' << std::endl;
	}

	// write occurrences:
	ofile_occ << "The fraction of motif occurrence:" << std::endl
			  << occ_frac_<< std::endl
	 	 	  << "The number of motif occurrences per sequence:" << std::endl
	 	 	  << occ_mult_ << std::endl;

}

void FDR::writePvalues( int n ){

	std::string opath = std::string( Global::outputDirectory ) + '/'
			+ Global::posSequenceBasename + "_motif_" + std::to_string( n+1 );

	std::string opath_zoops = opath + ".zoops.pvalues";
	std::string opath_mops = opath + ".mops.pvalues";

	std::ofstream ofile_zoops( opath_zoops );
	std::ofstream ofile_mops( opath_mops );

	for( size_t i = 0; i < ZOOPS_Pvalue_.size(); i++ ){
		ofile_zoops << std::setprecision(6)
					<< ZOOPS_Pvalue_[i] << std::endl;
	}

	for( size_t i = 0; i < MOPS_Pvalue_.size(); i++ ){
		ofile_mops  << std::setprecision(6)
					<< MOPS_Pvalue_[i] << std::endl;
	}

}
void FDR::writeLogOdds( int n ){

	/**
	 * save log odds scores into two files:
	 * (1) posSequenceBasename.zoops.logOdds
	 * (2) posSequenceBasename.mops.logOdds
	 */

	std::string opath = std::string( Global::outputDirectory ) + '/'
			+ Global::posSequenceBasename + "_motif_" + std::to_string( n+1 );

	std::string opath_zoops_logOdds = opath + ".zoops.logOdds";
	std::string opath_mops_logOdds = opath + ".mops.logOdds";
	std::ofstream ofile_zoops_logOdds( opath_zoops_logOdds );
	std::ofstream ofile_mops_logOdds( opath_mops_logOdds );

	size_t i;
	int M;
	if( Global::negSeqGiven ){
		M = Global::negSequenceSet->getN() / Global::posSequenceSet->getN();
	} else {
		M = Global::mFold;
	}

	ofile_zoops_logOdds << "positive" << '\t'
						<< "negative" << std::endl;
	for( i = 0; i < posScoreMax_.size(); i++ ){
		ofile_zoops_logOdds	<< std::setprecision(6)
							<< posScoreMax_[i] << '\t'
							<< negScoreMax_[i*M] << std::endl;
	}

	ofile_mops_logOdds 	<< "positive" << '\t'
						<< "negative" << std::endl;
	for( i = 0; i < posScoreAll_.size(); i++ ){
		ofile_mops_logOdds 	<< std::setprecision(6)
							<< posScoreAll_[i] << '\t'
							<< negScoreAll_[i*M] << std::endl;
	}

}
