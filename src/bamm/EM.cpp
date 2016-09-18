#include "EM.h"

EM::EM( Motif* motif, BackgroundModel* bg, std::vector<int> folds ){

	motif_ = motif;
	bg_ = bg;

	int y, k, j, L;
	int W = motif_->getW();

	for( int k = 0; k < Global::modelOrder+2; k++ ){
		Y_.push_back( ipow( Alphabet::getSize(), k ) );
	}

	if( folds.empty() ){										// for the complete sequence set
		folds_.resize( Global::cvFold );
		std::iota( std::begin( folds_ ), std::end( folds_ ), 0 );
	} else {													// for cross-validation
		folds_ = folds;
	}

	// copy selected positive sequences due to folds
	posSeqs_.clear();
	for( size_t f_idx = 0; f_idx < folds_.size(); f_idx++ ){
		for( size_t s_idx = 0; s_idx < Global::posFoldIndices[folds_[f_idx]].size(); s_idx++ ){
			posSeqs_.push_back( Global::posSequenceSet->getSequences()[Global::posFoldIndices[folds_[f_idx]][s_idx]] );
		}
	}

	// allocate memory for s_[y][j] and initialize it
	s_ = ( float** )calloc( Y_[Global::modelOrder+1], sizeof( float* ) );
	for( y = 0; y < Y_[Global::modelOrder+1]; y++ ){
		s_[y] = ( float* )calloc( W, sizeof( float ) );
	}

	// allocate memory for r_[n][i] and initialize it
	r_ = ( float** )calloc( posSeqs_.size(), sizeof( float* ) );
	for( size_t n = 0; n < posSeqs_.size(); n++ ){
		if( Global::setSlow ){
			L = posSeqs_[n]->getL() - W + 1;
		} else {
			L = posSeqs_[n]->getL();
		}
		r_[n] = ( float* )calloc( L, sizeof( float ) );
	}

	// allocate memory for n_[k][y][j] and probs_[k][y][j] and initialize them
	n_ = ( float*** )calloc( Global::modelOrder+1, sizeof( float** ) );
	for( k = 0; k < Global::modelOrder+1; k++ ){
		n_[k] = ( float** )calloc( Y_[k+1], sizeof( float* ) );
		for( y = 0; y < Y_[k+1]; y++ ){
			n_[k][y] = ( float* )calloc( W, sizeof( float ) );
		}
	}

	// allocate memory for alpha_[k][j] and initialize it
	alpha_ = ( float** )malloc( ( Global::modelOrder+1 ) * sizeof( float* ) );
	for( k = 0; k < Global::modelOrder+1; k++ ){
		alpha_[k] = ( float* )malloc( W * sizeof( float ) );
		for( j = 0; j < W; j++ ){
			alpha_[k][j] = Global::modelAlpha[k];
		}
	}


}

EM::~EM(){

	for( int y = 0; y < Y_[Global::modelOrder+1]; y++ ){
		free( s_[y] );
	}
	free( s_ );

	for( size_t n = 0; n < posSeqs_.size(); n++ ){
		free( r_[n] );
	}
	free( r_ );

	for( int k = 0; k < Global::modelOrder+1; k++ ){
		for( int y = 0; y < Y_[k+1]; y++ ){
			free( n_[k][y] );
		}
		free( n_[k] );
	}
	free( n_ );

	for( int k = 0; k < Global::modelOrder+1; k++ ){
		free( alpha_[k] );
	}
	free( alpha_ );

}

int EM::learnMotif(){

	printf( " ______\n"
			"|      |\n"
			"|  EM  |\n"
			"|______|\n\n" );

	bool iterate = true;									// flag for iterating before convergence
	int W = motif_->getW();
	int K_model = Global::modelOrder;
	int K_bg = Global::bgModelOrder;

	int y, y_bg, j;
	float v_diff, llikelihood_prev, llikelihood_diff = 0.0f;
	float** v_prev;											// hold the parameters of the highest-order before EM

	// allocate memory for parameters v[y][j] with the highest order
	v_prev = ( float** )calloc( Y_[Global::modelOrder+1], sizeof( float* ) );
	for( y = 0; y < Y_[Global::modelOrder+1]; y++ ){
		v_prev[y] = ( float* )calloc( W, sizeof( float ) );
	}

	// iterate over
	while( iterate && ( EMIterations_ < Global::maxEMIterations ) ){

		EMIterations_++;

		// before EM, get parameter variables before EM
		for( y = 0; y < Y_[Global::modelOrder+1]; y++ ){
			for( j = 0; j < W; j++ ){
				v_prev[y][j] = motif_->getV()[Global::modelOrder][y][j];
			}
		}
		llikelihood_prev = llikelihood_;

		// compute log odd scores s[y][j], log likelihoods of the highest order K
		for( y = 0; y < Y_[K_model+1]; y++ ){
			for( j = 0; j < W; j++ ){
				y_bg = y % Y_[K_bg+1];
				s_[y][j] = logf( motif_->getV()[K_model][y][j] ) - logf( bg_->getV()[K_bg][y_bg] );
			}
		}

		// E-step: calculate posterior
		EStep();

		// M-step: update parameters
		MStep();

		// * optional: optimize parameter alpha
		if( !Global::noAlphaOptimization )	optimizeAlphas();

		// * optional: optimize parameter q
		if( !Global::noQOptimization )		optimizeQ();

		// check likelihood for convergence
		v_diff = 0.0f;									 	// reset difference of posterior probabilities

		for( y = 0; y < Y_[Global::modelOrder+1]; y++ ){
			for( j = 0; j < W; j++ ){
				v_diff += fabsf( motif_->getV()[Global::modelOrder][y][j] - v_prev[y][j] );
			}
		}
		llikelihood_diff = llikelihood_ - llikelihood_prev;

		if( Global::verbose ){
			std::cout << "At " << EMIterations_ << "th iteration:	";
			std::cout << "para_diff = " << v_diff << ",	";
			std::cout << "log likelihood = " << llikelihood_ << " 	";
			if( llikelihood_diff < 0 && EMIterations_ > 1 ) std::cout << " decreasing...";
			std::cout << std::endl;
		}
		if( v_diff < Global::epsilon )					iterate = false;
		if( llikelihood_diff < 0 && EMIterations_ > 1 )	iterate = false;
	}

	// calculate probabilities
	motif_->calculateP();

	// free memory
	for( y = 0; y < Y_[Global::modelOrder+1]; y++ ){
		free( v_prev[y] );
	}
	free( v_prev );

    return 0;
}

void EM::EStep(){

	int L, LW1, k, y, j, i;
	float prior_i;											// positional preference prior state for p(z_n = i), motif present
	float prior_0 = 1 - q_;									// positional preference prior state for p(z_n = 0), no motif present
	float normFactor;										// normalize responsibilities r[n][i]
	int K_model = Global::modelOrder;
	int W = motif_->getW();
	llikelihood_ = 0.0f;									// reset log likelihood

	// calculate responsibilities r_[n][i] at position i in sequence n
	if( Global::setSlow ){
		// slow code:
		for( size_t n = 0; n < posSeqs_.size(); n++ ){		// n runs over all sequences
			L = posSeqs_[n]->getL();
			LW1 = L - W + 1;
			normFactor = 0.0f;								// reset normalization factor

			// reset r_[n][i]
			for( i = 0; i < LW1; i++ ){
				r_[n][i] = 0.0f;
			}

			// when p(z_n > 0)
			prior_i = q_ / static_cast<float>( LW1 );		// p(z_n = i), i > 0
			for( i = 0; i < LW1; i++ ){
				for( j = 0; j < W; j++ ){
					k = std::min( i+j, K_model );
					y = posSeqs_[n]->extractKmer( i+j, k );
					if( y != -1 ){							// skip 'N' and other unknown alphabets
						r_[n][i] += s_[y][j];
					} /*else if ( j < K_model ){			// for N exists and j < K occasions
						y = posSeqs_[n]->extractKmer( i+j, j );
						if( y != -1 ){
							r_[n][i] += s_[y][j];
						} else {
							r_[n][i] = 0.0f;
							break;
						}
					}*/ else {
						r_[n][i] = 0.0f;
						break;
					}
				}
				if( r_[n][i] != 0.0f ){
					r_[n][i] = expf( r_[n][i] ) * prior_i;
				}
				normFactor += r_[n][i];
			}

			// when p(z_n = 0)
			normFactor += prior_0;

			for( i = 0; i < LW1; i++ ){						// responsibility normalization
				r_[n][i] /= normFactor;
			}

			// only for testing: print out posterior parameter r:
//			for( i = 0; i < LW1; i++ ){
//				std::cout << std::scientific << std::setprecision(6) << "r["<< n+1 << "][" << i << "] = " << r_[n][i] << ' ';
//			}
//			std::cout << std::endl;

			llikelihood_ += logf( normFactor );
		}
	} else {
		// fast code:
		for( size_t n = 0; n < posSeqs_.size(); n++ ){		// n runs over all sequences
			L = posSeqs_[n]->getL();
			LW1 = L - W + 1;
			normFactor = 0.0f;								// reset normalization factor

			// reset r_[n][i]
			for( i = 0; i < L; i++ ){
				r_[n][i] = 0.0f;
			}

			// when p(z_n > 0)
			prior_i = q_ / static_cast<float>( LW1 );		// p(z_n = i), i > 0
			for( i = 0; i < L; i++ ){						// i runs over all nucleotides in sequence
				k = std::min( i, K_model );
				y = posSeqs_[n]->extractKmer( i, k );		// extract (k+1)-mer y from positions (i-k,...,i)
				for( j = 0; j < std::min( W, i+1 ); j++ ){	// j runs over all motif positions
					if( y != -1 ){							// skip 'N' and other unknown alphabets
						r_[n][L-i+j-1] += s_[y][j];
					}/* else if( j < K_model ){
						y = posSeqs_[n]->extractKmer( i, K_model-j );
						if( y != -1 ){
							r_[n][L-i+j-1] += s_[y][j];
						} else {
							r_[n][L-i+j-1] = 0.0f;
							break;
						}
					}*/ else {
						r_[n][L-i+j-1] = 0.0f;
						break;
					}
				}
			}
			for( i = W-1; i < L; i++ ){
				if( r_[n][i] != 0.0f ){
					r_[n][i] = expf( r_[n][i] ) * prior_i;
				}
				normFactor += r_[n][i];
			}

			// when p(z_n = 0)
			normFactor += prior_0;

			// normalize responsibilities
			for( i = W-1; i < L; i++ ){
				r_[n][i] /= normFactor;
			}

			// only for testing: print out posterior parameter r:
//			for( i = L-1; i >= W-1; i-- ){
//				std::cout << std::scientific << std::setprecision(6) << "r["<< n+1 << "][" << i << "] = " << r_[n][i] << ' ';
//			}
//			std::cout << std::endl;

			llikelihood_ += logf( normFactor );
		}
	}
}

void EM::MStep(){

	int L, LW1, k, y, y2, j, i;
	int W = motif_->getW();

	// reset the fractional counts n
	for( k = 0; k < Global::modelOrder+1; k++ ){
		for( y = 0; y < Y_[k+1]; y++ ){
			for( j = 0; j < W; j++ ){
				n_[k][y][j] = 0.0f;
			}
		}
	}

	// compute fractional occurrence counts for the highest order K
	if( Global::setSlow ){
		// slow code:
		for( size_t n = 0; n < posSeqs_.size(); n++ ){		// n runs over all sequences
			L = posSeqs_[n]->getL();
			LW1 = L - W + 1;
			for( i = 0; i < LW1; i++ ){						// i runs over all nucleotides in sequence
				for( j = 0; j < W; j++ ){					// j runs over all motif positions
					k = std::min( i+j, Global::modelOrder );
					y = posSeqs_[n]->extractKmer( i+j, k );
					if( y != -1 ){							// skip 'N' and other unknown alphabets
						n_[Global::modelOrder][y][j] += r_[n][i];
					}
				}
			}
		}
	} else {
		// fast code:
		for( size_t n = 0; n < posSeqs_.size(); n++ ){		// n runs over all sequences
			L = posSeqs_[n]->getL();
			LW1 = L - W + 1;
			for( i = 0; i < L; i++ ){
				k =  std::min( i, Global::modelOrder );
				y = posSeqs_[n]->extractKmer( i, k );
				for( j = 0; j < std::min( W, i+1 ); j++ ){
					if( y != -1 && ( i-j ) < LW1 ){			// skip 'N' and other unknown alphabets
						n_[Global::modelOrder][y][j] += r_[n][L-i+j-1];
					}
				}
			}
		}
	}

	// compute fractional occurrence counts from higher to lower order
	for( k = Global::modelOrder; k > 0; k-- ){				// k runs over all orders
		for( y = 0; y < Y_[k+1]; y++ ){
			y2 = y % Y_[k];									// cut off the first nucleotide in (k+1)-mer
			for( j = 0; j < W; j++ ){
				n_[k-1][y2][j] += n_[k][y][j];
			}
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
	// optimize hyper-parameter q
	// motif.updateV()
}

float EM::calculateQfunc(){

	int L, LW1, i, k, y, j;
	float sumS = 0.0f, Qfunc = 0.0f;
	float prior_i, prior_0 = 1 - q_;
	int W = motif_->getW();

	for( size_t n = 0; n < posSeqs_.size(); n++ ){
		L = posSeqs_[n]->getL();
		LW1 = L - W + 1;
		prior_i = q_ / static_cast<float>( LW1 );
		for( i = 0; i <= LW1; i++ ){
			for( j = 0; j < W; j++ ){
				k = std::min( j, Global::modelOrder );
				y =  posSeqs_[n]->extractKmer( i, k );
				if( y != -1 ){
					sumS += s_[y][j];
				}
			}
			Qfunc += r_[n][i] * sumS;
		}
		Qfunc += ( r_[n][0] * logf( prior_0 ) + ( 1 - r_[n][0] ) * logf( prior_i ) );
	}
	return Qfunc;
}

void EM::print(){

	std::cout << " ____________________________________" << std::endl;
	std::cout << "|                                    |" << std::endl;
	std::cout << "| Conditional probabilities after EM |" << std::endl;
	std::cout << "|____________________________________|" << std::endl;
	int W = motif_->getW();
	for( int j = 0; j < W; j++ ){
		for( int k = 0; k < Global::modelOrder+1; k++ ){
			for( int y = 0; y < Y_[k+1]; y++ ){
				std::cout << std::scientific << motif_->getV()[k][y][j] << '\t';
			}
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}
}

void EM::write(){

	/**
	 * save EM parameters in four flat files:
	 * (1) posSequenceBasename.EMcounts:	refined counts of (k+1)-mers
	 * (2) posSequenceBasename.EMposterior: responsibilities, posterior distributions
	 * (3) posSequenceBasename.EMalpha:		hyper-parameter alphas
	 * (4) posSequenceBasename.EMlogScores:	log scores
	 */

	int k, y, j, i, L;
	int W = motif_->getW();

	std::string opath = std::string( Global::outputDirectory ) + '/'
						+ std::string( Global::posSequenceBasename );

	// output (k+1)-mer counts n[k][y][j]
	std::string opath_n = opath + ".EMcounts";
	std::ofstream ofile_n( opath_n.c_str() );
	for( j = 0; j < W; j++ ){
		for( k = 0; k < Global::modelOrder+1; k++ ){
			for( y = 0; y < Y_[k+1]; y++ ){
				ofile_n << std::scientific << n_[k][y][j] << ' ';
			}
			ofile_n << std::endl;
		}
		ofile_n << std::endl;
	}

	// output responsibilities r[n][i]
	std::string opath_r = opath + ".EMposterior";
	std::ofstream ofile_r( opath_r.c_str() );
	for( size_t n = 0; n < posSeqs_.size(); n++ ){
		L = posSeqs_[n]->getL();
		if( Global::setSlow ){
			for( i = 0; i < L-W+1; i++ ){
				ofile_r << std::scientific << r_[n][i] << ' ';
			}
		} else {
			for( i = L-1; i > W-2; i-- ){
				ofile_r << std::scientific << r_[n][i] << ' ';
			}
		}
		ofile_r << std::endl;
	}

	// output parameter alphas alpha[k][j]
	std::string opath_alpha = opath + ".EMalpha";
	std::ofstream ofile_alpha( opath_alpha.c_str() );
	for( k = 0; k < Global::modelOrder+1; k++ ){
		ofile_alpha << "k = " << k << std::endl;
		for( j = 0; j < W; j++ ){
			ofile_alpha << std::setprecision( 1 ) << alpha_[k][j] << ' ';
		}
		ofile_alpha << std::endl;
	}

	// output log scores s[y][j]
	std::string opath_s = opath + ".EMlogScores";
	std::ofstream ofile_s( opath_s.c_str() );
	for( y = 0; y < Y_[Global::modelOrder+1]; y++ ){
		for( j = 0; j < W; j++ ){
			ofile_s << std::fixed << std::setprecision( 6 ) << s_[y][j] << '	';
		}
		ofile_s << std::endl;
	}
}
