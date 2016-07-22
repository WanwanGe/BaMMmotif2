/*
 * EM.cpp
 *
 *  Created on: Apr 18, 2016
 *      Author: administrator
 */

#include "EM.h"

EM::EM( Motif* motif, BackgroundModel* bg, std::vector<int> folds ){
	motif_ = motif;														// deep copy
	bg_ = bg;
	W_ = motif_->getW();

	if( folds.size() != 0 )												// for cross-validation
		folds_ = folds;

	int n, y, k, j;
	// allocate memory for s_[y][j] and initialize it
	s_ = ( float** )calloc( Global::powA[K_+1], sizeof( float* ) );
	for( y = 0; y < Global::powA[K_+1]; y++ )
		s_[y] = ( float* )calloc( W_, sizeof( float ) );

    // allocate memory for r_[n][i] and initialize it
	r_ = ( float** )calloc( Global::posSequenceSet->getN(), sizeof( float* ) );
    for( n = 0; n < Global::posSequenceSet->getN(); n++ )
    	r_[n] = ( float* )calloc( Global::posSequenceSet->getSequences()[n].getL(), sizeof( float ) );	// for fast code

    // allocate memory for n_[k][y][j] and initialize it
	n_ = ( float*** )calloc( K_+1, sizeof( float** ) );
	for( k = 0; k < K_+1; k++ ){
		n_[k] = ( float** )calloc( Global::powA[k+1], sizeof( float* ) );
		for( y = 0; y < Global::powA[k+1]; y++ )
			n_[k][y] = ( float* )calloc( W_, sizeof( float ) );
	}

	// allocate memory for alpha_[k][j] and initialize it
	alpha_ = ( float** )malloc( ( K_+1 ) * sizeof( float* ) );
	for( k = 0; k < K_+1; k++ ){
		alpha_[k] = ( float* )malloc( W_ * sizeof( float ) );
		for( j = 0; j < W_; j++ )										// initialize alpha_ with global alpha
			alpha_[k][j] = Global::modelAlpha[k];
	}
}

EM::~EM(){

	for( int y = 0; y < Global::powA[K_+1]; y++ )
		free( s_[y] );
	free( s_ );

	for( int n = 0; n < Global::posSequenceSet->getN(); n++ )
		free( r_[n] );
	free( r_ );

	for( int k = 0; k < K_+1; k++ ){
		for( int y = 0; y < Global::powA[k+1]; y++ )
			free( n_[k][y] );
		free( n_[k] );
	}
	free( n_ );

	for( int k = 0; k < K_+1; k++ )
		free( alpha_[k] );
	free( alpha_ );
	std::cout << "Destructor for EM class works fine. \n";
}

int EM::learnMotif(){

	printf( " ______\n"
			"|      |\n"
			"|  EM  |\n"
			"|______|\n\n" );

	bool iterate = true;												// flag for iterating before convergence
	float difference;													// model parameter difference for each EM iteration
	float** v_prior;													// hold the parameters before EM
	float likelihood_prior, likelihood_diff;

	int y, j;

	// allocate memory for prior and posterior parameters v[y][j] with the highest order
	v_prior = ( float** )calloc( Global::powA[K_+1], sizeof( float* ) );
	for( y = 0; y < Global::powA[K_+1]; y++ )
		v_prior[y] = ( float* )calloc( W_, sizeof( float ) );

	// iterate over
	while( iterate && ( EMIterations_ < Global::maxEMIterations ) ){

		EMIterations_++;

		// before EM, get the prior parameter variables
		for( y = 0; y < Global::powA[K_+1]; y++ )
			for( j = 0; j < W_; j++ )
				v_prior[y][j] = motif_->getV()[K_][y][j];
		likelihood_prior = likelihood_;

		// E-step: calculate posterior
		EStep();

		// M-step: update parameters
		MStep();

		// * optional: optimizeAlphas()
		if( !Global::noAlphaOptimization )	optimizeAlphas();

		// * optional: optimizeQ()
		if( !Global::noQOptimization )		optimizeQ();

		// * check likelihood for convergence

		difference = 0.0f;												// reset difference
		for( y = 0; y < Global::powA[K_+1]; y++ )
			for( j = 0; j < W_; j++ )
				difference += fabs( motif_->getV()[K_][y][j] - v_prior[y][j] );

		likelihood_diff = likelihood_ - likelihood_prior;
		if( Global::verbose ){
			std::cout << "At " << EMIterations_ << "th iteration:\n";
			std::cout << "	- difference = " << difference << "\n";
			std::cout << "	- likelihood = " << likelihood_;
			if( likelihood_diff < 0 ) std::cout << " decreasing...";
			std::cout << std::endl;
		}

		if( difference < Global::epsilon )	iterate = false;
	}

/*
    // * remark: iterate over sequences using Global::posFoldIndices and folds_
	if( folds_.size() > 0 )
		for( unsigned int f = 0; f < folds_.size() ; f++ ){
			int fold = folds_[f];
			for( unsigned int n = 0; n < Global::posFoldIndices[fold].size(); n++ )
				Sequence seq = Global::posSequenceSet->getSequences()[Global::posFoldIndices[fold][n]];
		}
*/

//	if( Global::verbose ) print();

	for( y = 0; y < Global::powA[K_+1]; y++ )
		free( v_prior[y] );
	free( v_prior );

    return 0;
}

void EM::EStep(){

	int L, y, j, y3, n, i;
	float prior_i;														// positional preference prior state for p(z_n = i), motif present
	float prior_0 = 1 - q_;												// positional preference prior state for p(z_n = 0), no motif present
	float normFactor;													// normalize responsibilities r[n][i]
	likelihood_ = 0.0f;													// reset likelihood

	// compute log odd scores s[y][j], log likelihoods with the highest order K
	for( y = 0; y < Global::powA[K_+1]; y++ ){
		for( j = 0; j < W_; j++ ){
			if( K_ <= k_bg_ ){
				s_[y][j] = log( motif_->getV()[K_][y][j]
				              / bg_->getVbg()[K_][y] );
			} else {
				y3 = y % Global::powA[3];								// 3 rightmost nucleotides in (k+1)-mer y
				s_[y][j] = log( motif_->getV()[K_][y][j]
				              / bg_->getVbg()[k_bg_][y3] );
			}
		}
	}

	// calculate responsibilities r_[n][i] at position i in sequence n

//	// slow code:
//	for( n = 0; n < Global::posSequenceSet->getN(); n++ ){				// n runs over all sequences
//
//		Sequence seq = Global::posSequenceSet->getSequences()[n];
//		LW1 = seq.getL() - W_ + 1;
//		normFactor = 0.0f;												// reset normalization factor
//
//		// when p(z_n = 0)
//		r_[n][0] = prior_0;
//		normFactor += r_[n][0];
//
//		// when p(z_n > 0)
//		prior_i = q_ / LW1;												// p(z_n = i), i > 0
//		for( i = 1; i <= LW1; i++ ){
//			for( j = 0; j < W_; j++ ){
//				y = seq.extractKmer( i+j-1, ( j < K_ ) ? j : K_ );
//				if( y != -1 )											// skip 'N' and other unknown alphabets
//					r_[n][i] += s_[y][j];
//				else
//					break;
//			}
//			likelihood_ += exp( r_[n][i] );
//			r_[n][i] = exp( r_[n][i] ) * prior_i;
//			normFactor += r_[n][i];
//		}
//
//		// responsibility normalization
//		for( i = 0; i <= LW1; i++ )
//			r_[n][i] /= normFactor;
//	}

	// fast code:
	for( n = 0; n < Global::posSequenceSet->getN(); n++ ){				// n runs over all sequences
		Sequence seq = Global::posSequenceSet->getSequences()[n];
		L = seq.getL();
		normFactor = 0.0f;

		// when p(z_n > 0)
		prior_i = q_ / (L - W_ + 1);
		for( i = 0; i < L; i++ ){										// i runs over all nucleotides in sequence
			y = seq.extractKmer( i, ( i < K_ ) ? i : K_ );				// extract (k+1)-mer y from positions (i-k,...,i)
			if( y != -1 )												// skip 'N' and other unknown alphabets
				for( j = 0; j < ( ( W_ < i ) ? W_ : i ); j++ )			// j runs over all motif positions
						r_[n][L-i+j] += s_[y][j];
			else
				break;
		}

		for( i = 1; i <= L; i++ ){
			likelihood_ +=  exp( r_[n][i] );
			r_[n][i] = exp( r_[n][i] ) * prior_i;
			normFactor += r_[n][i];
		}

		// when p(z_n = 0)
		r_[n][0] = prior_0;
		normFactor += r_[n][0];

		likelihood_ += normFactor;

		for( i = 0; i <= L; i++ )										// normalization
			r_[n][i] /= normFactor;
	}
}


void EM::MStep(){

	int L, k, y, y2, j, n, i;

	// reset the fractional counts n
	for( k = 0; k < K_+1; k++ )
		for( y = 0; y < Global::powA[k+1]; y++ )
			for( j = 0; j < W_; j++ )
				n_[k][y][j] = 0.0f;

	// compute fractional occurrence counts for the highest order K
	// slow code:
//	for( n = 0; n < Global::posSequenceSet->getN(); n++ ){				// n runs over all sequences
//		Sequence seq = Global::posSequenceSet->getSequences()[n];
//		LW1 = seq.getL() - W_ + 1;
//		for( i = 1; i <= LW1; i++ ){									// i runs over all nucleotides in sequence
//			for( j = 0; j < W_; j++ ){									// j runs over all motif positions
//				y = seq.extractKmer( i+j-1, ( j < K_ ) ? j : K_ );		// extract (k+1)-mer y
//				if( y != -1 )											// skip 'N' and other unknown alphabets
//					n_[K_][y][j] += r_[n][i];
//				else
//					break;
//			}
//		}
//	}

	// fast code:
	for( n = 0; n < Global::posSequenceSet->getN(); n++ ){				// n runs over all sequences
		Sequence seq = Global::posSequenceSet->getSequences()[n];
		L = seq.getL();
		for( i = 0; i < L; i++ ){
			y = seq.extractKmer( i, ( i < K_ ) ? i : K_ );
			for( j = 0; j < ( ( W_ < i ) ? W_ : i ); j++ ){
				if( y != -1 )											// skip 'N' and other unknown alphabets
					n_[K_][y][j] += r_[n][L-i+j];
				else
					break;
			}
		}
	}

	// compute fractional occurrence counts for lower orders
	for( k = K_; k > 0; k-- ){											// k runs over all orders
		for( y = 0; y < Global::powA[k+1]; y++ ){
			y2 = y % Global::powA[k];									// cut off the first nucleotide in (k+1)-mer
			for( j = 0; j < W_; j++ )
				n_[k-1][y2][j] += n_[k][y][j];
		}
	}

	// update model parameters v[k][y][j]
	motif_->updateV( n_, alpha_ );
}

void EM::optimizeAlphas(){
	// optimize alphas
	// motif.updateV()
}

void EM::optimizeQ(){
	// optimize Q function
	// motif.updateV()
}

void EM::print(){
	for( int j = 0; j < W_; j++ ){
		for( int k = 0; k < K_+1; k++ ){
			for( int y = 0; y < Global::powA[k+1]; y++ )
				std::cout << std::scientific << motif_->getV()[k][y][j] << '\t';
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}
}

void EM::write(){

	/*
	 * save EM parameters in five flat files:
	 * (1) posSequenceBasename.EMconds: 	conditional probabilities
	 * (2) posSequenceBasename.EMcounts:	refined counts of (k+1)-mers
	 * (3) posSequenceBasename.EMposterior: responsibilities, posterior distributions
	 * (4) posSequenceBasename.EMalpha:		hyper-parameter alphas
	 * (5) posSequenceBasename.EMlogOdds:	log odds scores
	 */

	std::string opath = std::string( Global::outputDirectory ) + '/'
			+ std::string( Global::posSequenceBasename );

	// output conditional probabilities v[k][y][j] and (k+1)-mer counts n[k][y][j]
	std::string opath_v = opath + ".EMconds";
	std::string opath_n = opath + ".EMcounts";
	std::ofstream ofile_v( opath_v.c_str() );
	std::ofstream ofile_n( opath_n.c_str() );
	for( int j = 0; j < W_; j++ ){
		for( int k = 0; k < K_+1; k++ ){
			for( int y = 0; y < Global::powA[k+1]; y++ ){
				ofile_v << std::scientific << motif_->getV()[k][y][j] << '\t';
				ofile_n << std::scientific << n_[k][y][j] << '\t';
			}
			ofile_v << std::endl;
			ofile_n << std::endl;
		}
		ofile_v << std::endl;
		ofile_n << std::endl;
	}

	// output responsibilities r[n][i]
	std::string opath_r = opath + ".EMposterior";
	std::ofstream ofile_r( opath_r.c_str() );
	for( int n = 0; n < Global::posSequenceSet->getN(); n++ ){
		Sequence seq = Global::posSequenceSet->getSequences()[n];
		int L = seq.getL();
		for( int i = 0; i <= L/*-W_*/+1; i++ )
			ofile_r << std::scientific << r_[n][i] << '\t';
		ofile_r << std::endl;
	}

	// output parameter alphas alpha[k][j]
	std::string opath_alpha = opath + ".EMalpha";
	std::ofstream ofile_alpha( opath_alpha.c_str() );
	for( int k = 0; k < K_+1; k++ ){
		ofile_alpha << "k = " << k << std::endl;
		for( int j = 0; j < W_; j++ )
			ofile_alpha << std::scientific << alpha_[k][j] << '\t';
		ofile_alpha << std::endl;
	}

	// output log odds scores s[y][j]
	std::string opath_s = opath + ".EMlogOdds";
	std::ofstream ofile_s( opath_s.c_str() );
	for( int y = 0; y < Global::powA[K_+1]; y++ ){
		for( int j = 0; j < W_; j++ )
			ofile_s << std::fixed << std::setprecision(6) << s_[y][j] << '\t';
		ofile_s << std::endl;
	}
}
