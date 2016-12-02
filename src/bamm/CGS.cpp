#include "CGS.h"
#include "EM.h"

#include <random>			// std::discrete_distribution

CGS::CGS( Motif* motif, BackgroundModel* bg, std::vector<int> folds ){

	motif_ = motif;
	bg_ = bg;

	for( int k = 0; k < std::max( Global::modelOrder+2,  Global::bgModelOrder+2 ); k++ ){
		Y_.push_back( ipow( Alphabet::getSize(), k ) );
	}

	int W = motif_->getW();
	int LW1 = Global::posSequenceSet->getMaxL() - W + 1;
	int N = Global::posSequenceSet->getN();

	// allocate memory for counts nz_[k][y][j]
	n_z_ = ( int*** )calloc( Global::modelOrder+1, sizeof( int** ) );
	for( int k = 0; k < Global::modelOrder+1; k++ ){
		n_z_[k] = ( int** )calloc( Y_[k+1], sizeof( int* ) );
		for( int y = 0; y < Y_[k+1]; y++ ){
			n_z_[k][y] = ( int* )calloc( W, sizeof( int ) );
		}
	}

	// allocate memory and initialize responsibility r_[n][i]
	r_ = ( float** )calloc( N, sizeof( float* ) );
	for( int n = 0; n < N; n++ ){
		r_[n] = ( float* )calloc( LW1+1, sizeof( float ) );
	}

	// allocate memory for s_[y][j] and initialize it
	s_ = ( float** )calloc( Y_[Global::modelOrder+1], sizeof( float* ) );
	for( int y = 0; y < Y_[Global::modelOrder+1]; y++ ){
		s_[y] = ( float* )calloc( W, sizeof( float ) );
	}

	// allocate memory and initialize alpha_[k][j]
	alpha_ = ( float** )malloc( ( Global::modelOrder+1 ) * sizeof( float* ) );
	for( int k = 0; k < Global::modelOrder+1; k++ ){
		alpha_[k] = ( float* )malloc( W * sizeof( float ) );
		for( int j = 0; j < W; j++ ){
			alpha_[k][j] = Global::modelAlpha[k];
		}
	}

	// allocate memory for positional prior pos_[i]
	pos_ = ( float* )calloc( LW1+1, sizeof( float ) );

//	EM em( motif_, bg_ );
//	em.learnMotif();
	// allocate memory for motif position z_[n]
	z_ = ( int* )calloc( N, sizeof( int ) );
	for( int n = 0; n < Global::posSequenceSet->getN(); n++ ){
		z_[n] = rand() % ( Global::posSequenceSet->getMinL() - W + 2 );
//		z_[n] = em.getZ()[n];
	}

}

CGS::~CGS(){

	for( int k = 0; k < Global::modelOrder+1; k++ ){
		for( int y = 0; y < Y_[k+1]; y++ ){
			free( n_z_[k][y] );
		}
		free( n_z_[k] );
		free( alpha_[k] );
	}
	free( n_z_ );
	free( alpha_ );

	for( int n = 0; n < Global::posSequenceSet->getN(); n++ ){
		free( r_[n] );
	}
	free( r_ );
	for( int y = 0; y < Y_[Global::modelOrder+1]; y++ ){
		free( s_[y] );
	}
	free( s_ );

	free( pos_ );
	free( z_ );

}

void CGS::GibbsSampling(){

	printf( " ___________________________\n"
			"|                           |\n"
			"|  Collapsed Gibbs sampler  |\n"
			"|___________________________|\n\n" );

	clock_t t0 = clock();
	bool iterate = true;								// flag for iterating before convergence

	int K = Global::modelOrder;
	int W = motif_->getW();
	int k, j;

	// parameters for alpha learning
	int timestep = 0;									// timestep = iteration
	float eta = 0.001f;									// learning rate for alpha
	float beta1 = 0.9f;									// exponential decay rate for the moment estimates
	float beta2 = 0.999f;								// exponential decay rate for the moment estimates
	float epsilon = 0.00000001f;						//
	float** gradient;									// gradient of log posterior of alpha
	float** m1;											// first moment vector (the mean)
	float** m2;											// second moment vector (the uncentered variance)
	float alpha_diff;
	gradient = ( float** )calloc( K+1, sizeof( float* ) );
	m1 = ( float** )calloc( K+1, sizeof( float* ) );
	m2 = ( float** )calloc( K+1, sizeof( float* ) );
	for( k = 0; k < K+1; k++ ){
		gradient[k] = ( float* )calloc( W, sizeof( float ) );
		m1[k] = ( float* )calloc( W, sizeof( float ) );
		m2[k] = ( float* )calloc( W, sizeof( float ) );
	}
	float m1_i = 0.0f;
	float m2_i = 0.0f;

/*
	float** v_prev;
	v_prev = ( float** )calloc( Y_[K+1], sizeof( float* ) );
	for( y = 0; y < Y_[K+1]; y++ ){
		v_prev[y] = ( float* )calloc( W, sizeof( float ) );
	}
	float v_diff;
*/

	// iterate over
	while( iterate && timestep < Global::maxCGSIterations ){

		timestep++;

		if( Global::verbose ){
			std::cout << timestep << " iteration:\t";
		}

		// sampling z and q
		sampling_z_q();

		// only for writing out model after each iteration:
/*		motif_->calculateP();
		motif_->write( CGSIterations_ );*/

		// update model parameter v
		motif_->updateV( n_z_, alpha_, K-1 );

		// optimize hyper-parameter alpha
//		if( !Global::noAlphaUpdating )	updateAlphas( eta );
		alpha_diff = 0.0f;

		for( k = 0; k < K+1; k++ ){

			for( j = 0; j < W; j++ ){

				// get gradients w.r.t. stochastic objective at timestep t
				gradient[k][j] = calcGrad_logPostAlphas( alpha_[k][j], k, j );

				// update biased first moment estimate
				m1_i = beta1 * m1_i + ( 1 - beta1 ) * gradient[k][j];

				// update biased second raw moment estimate
				m2_i = beta2 * m2_i + ( 1 - beta2 ) * gradient[k][j] * gradient[k][j];

				// compute bias-corrected first moment estimate
				m1[k][j] = m1_i / ( 1 - beta1 );

				// compute bias-corrected second raw moment estimate
				m2[k][j] = m2_i / ( 1 - beta2 );

				// update parameter alphas
				alpha_[k][j] = alpha_[k][j] - eta * m1[k][j] / ( ( float )sqrt ( m2[k][j] ) + epsilon );

				// calculate the changes
				alpha_diff += eta * m1[k][j] / ( ( float )sqrt ( m2[k][j] ) + epsilon );

			}
		}
		std::cout << alpha_diff << std::endl;
		if( alpha_diff < Global::epsilon ) iterate = false;

	}

	// obtaining a motif model

	// calculate probabilities
	motif_->calculateP();

	// free memory
	for( k = 0; k < K+1; k++ ){
		free( gradient[k]);
		free( m1[k] );
		free( m2[k] );
	}
	free( gradient );
	free( m1 );
	free( m2 );

	fprintf( stdout, "\n--- Runtime for Collapsed Gibbs sampling: %.4f seconds ---\n", ( ( float )( clock() - t0 ) ) / CLOCKS_PER_SEC );
}

void CGS::sampling_z_q(){

	int N = Global::posSequenceSet->getN();
	std::vector<Sequence*> posSeqs = Global::posSequenceSet->getSequences();
	int W = motif_->getW();
	int K = Global::modelOrder;
	int K_bg = Global::bgModelOrder;
	int n, n_prev, k, y, y_prev, y2, y_bg, i, j, LW1;
	int N_0 = 0;								// counts of sequences which do not contain motifs.

	// reset n_z_[k][y][j]
	for( k = 0; k < K+1; k++ ){
		for( y = 0; y < Y_[k+1]; y++ ){
			for( j = 0; j < W; j++ ){
				n_z_[k][y][j] = 0;
			}
		}
	}
	// count kmers for the highest order K
	for( i = 0; i < N; i++ ){
		for( j = 0; j < W; j++ ){
			y = posSeqs[i]->extractKmer( z_[i]-1+j, std::min( z_[i]-1+j, K ) );
			n_z_[K][y][j]++;
		}
	}
	// calculate nz for lower order k
	for( k = K; k > 0; k-- ){					// k runs over all lower orders
		for( y = 0; y < Y_[k+1]; y++ ){
			y2 = y % Y_[k];						// cut off the first nucleotide in (k+1)-mer
			for( j = 0; j < W; j++ ){
				n_z_[k-1][y2][j] += n_z_[k][y][j];
			}
		}
	}

	// sampling z:
	// loop over all sequences and drop one sequence each time and update r
	for( n = 0; n < N; n++ ){
		LW1 = posSeqs[n]->getL() - W + 1;

		// calculate positional prior:
		pos_[0] =  ( float )( 1.0f - q_ );
		for( i = 1; i <= LW1; i++ ){
			pos_[i] = ( float )q_ / ( float )LW1;
		}

		// count K-mers at position z[i]+j except the n'th sequence
		for( k = 0; k < K+1; k++ ){
			for( j = 0; j < W; j++ ){
				if( z_[n] != 0 ){
					// remove the kmer counts for the current sequence with old z
					y = posSeqs[n]->extractKmer( z_[n]-1+j, std::min( z_[n]-1+j, k ) );
					n_z_[k][y][j]--;
				}
				if( n > 0 ){
					n_prev = n-1;
					if( z_[n_prev] != 0 ){
						// add the kmer counts for the previous sequence with updated z
						y_prev = posSeqs[n-1]->extractKmer( z_[n_prev]-1+j, std::min( z_[n_prev]-1+j, k ) );
						n_z_[k][y_prev][j]++;
					}
				}
			}
		}

		// updated model parameters v excluding the n'th sequence
		motif_->updateV( n_z_, alpha_, K );

		// compute log odd scores s[y][j], log likelihoods of the highest order K
		for( y = 0; y < Y_[K+1]; y++ ){
			for( j = 0; j < W; j++ ){
				y_bg = y % Y_[K_bg+1];
				s_[y][j] = motif_->getV()[K][y][j] / bg_->getV()[std::min( K, K_bg )][y_bg];
			}
		}

		// sampling equation: calculate responsibilities over all LW1 positions on n'th sequence
		std::vector<float> posterior_array;
		float normFactor = 0.0f;
		for( i = 1; i <= LW1; i++ ){
			r_[n][i] = 1.0f;
			for( j = 0; j < W; j++ ){
				// extract k-mers on the motif at position i over W of the n'th sequence
				y = posSeqs[n]->extractKmer( i-1+j, std::min( i-1+j, K ) );
				r_[n][i] *= s_[y][j];
			}
			r_[n][i] *= pos_[i];
			normFactor += r_[n][i];
		}

		// for sequences that do not contain motif
		r_[n][0] = pos_[0];
		normFactor += r_[n][0];

		for( i = 0; i <= LW1; i++ ){
			r_[n][i] /= normFactor;
			posterior_array.push_back( r_[n][i] );
		}

		// draw a new position z from discrete posterior distribution
		std::discrete_distribution<> posterior_dist( posterior_array.begin(), posterior_array.end() );
		std::random_device rand;			// pick a random number
		z_[n] = posterior_dist( rand );		// draw a sample z

		if( z_[n] == 0 ) N_0++;
	}

	// sampling q:
	// draw two random numbers Q and P from Gamma distribution
	std::gamma_distribution<> P_Gamma_dist( N_0 + 1, 1 );
	std::gamma_distribution<> Q_Gamma_dist( N - N_0 + 1, 1 );
	std::random_device rand1;				// pick a random number
	std::random_device rand2;				// pick another random number
	double P = P_Gamma_dist( rand1 );		// draw a sample for P
	double Q = Q_Gamma_dist( rand2 );		// draw a sample for Q

	q_ = Q / ( Q + P );						// calculate q_
//	q_ = ( float )( N - N_0 ) / ( float )N;

	if( Global::verbose ){
		// checking z values from the first 20 sequences
		for( n = 0; n < 20; n++ ) std::cout << z_[n] << '\t';
		std::cout << N_0 << " sequences do not have motif. q = " << q_ << "\n";
	}
}

void CGS::updateAlphas( float eta ){

	// update alphas due to the learning rate eta and gradient of the log posterior of alphas

	int K = Global::modelOrder;
	int W = motif_->getW();

	int k, j;

	for( k = 0; k < K+1; k++ ){
		for( j = 0; j < W; j++ ){
			alpha_[k][j] -= eta * calcGrad_logPostAlphas( alpha_[k][j], k, j );
		}
	}

}

float CGS::calcGrad_logPostAlphas( float alpha, int k, int j ){

	// calculate gradient of the log posterior of alphas
	float gradient_logPostAlphas;
	float*** v = motif_->getV();
	float** v_bg = bg_->getV();
	int y, y2;

	// the first term
	gradient_logPostAlphas = ( -2.0f ) / alpha;
	// the second term
	gradient_logPostAlphas += Global::modelBeta * powf( Global::modelGamma, ( float )k ) / powf( alpha, 2.0f );
	// the third term
	gradient_logPostAlphas += ( float )Y_[k] * digammaf( alpha );
	// the forth term
	if( k > 0 ){
		for( y = 0; y < Y_[k+1]; y++ ){
			y2 = y % Y_[k];
			// the first term of the inner part
			if( j > 0 ){
				gradient_logPostAlphas += v[k-1][y2][j] * ( digammaf( ( float )n_z_[k][y][j] + alpha * v[k-1][y2][j] )
					- digammaf( alpha * v[k-1][y2][j] ) - digammaf( ( float )n_z_[k][y][j-1] + alpha ) + digammaf( alpha ) );
			} else {						// when j = 0
				gradient_logPostAlphas += v[k-1][y2][j] * ( digammaf( ( float )n_z_[k][y][j] + alpha * v[k-1][y2][j] )
					- digammaf( alpha * v[k-1][y2][j] ) );
			}
		}
	} else {	// when k = 0
		for( y = 0; y < Y_[1]; y++ ){
			if( j < 0 ){
				gradient_logPostAlphas += v_bg[k][y] * ( digammaf( ( float )n_z_[k][y][j] + alpha * v_bg[k][y] )
					- digammaf( alpha * v_bg[k][y] ) - digammaf( ( float )n_z_[k][y][j-1] + alpha ) + digammaf( alpha ) );
			} else {
				gradient_logPostAlphas += v_bg[k][y] * ( digammaf( ( float )n_z_[k][y][j] + alpha * v_bg[k][y] )
					- digammaf( alpha * v_bg[k][y] ) );
			}
		}
	}

	return gradient_logPostAlphas;
}

void CGS::print(){

}

void CGS::write(){

	/**
	 * save CGS parameters in three flat files:
	 * (1) posSequenceBasename.CGScounts:		refined fractional counts of (k+1)-mers
	 * (2) posSequenceBasename.CGSposterior:	responsibilities, posterior distributions
	 * (3) posSequenceBasename.CGSalpha:		hyper-parameter alphas
	 */

	int W = motif_->getW();
	std::vector<Sequence*> posSeqs = Global::posSequenceSet->getSequences();

	std::string opath = std::string( Global::outputDirectory ) + '/'
						+ std::string( Global::posSequenceBasename );

	// output (k+1)-mer counts nz[k][y][j]
	std::string opath_n = opath + ".CGScounts";
	std::ofstream ofile_n( opath_n.c_str() );
	for( int j = 0; j < W; j++ ){
		for( int k = 0; k < Global::modelOrder+1; k++ ){
			for( int y = 0; y < Y_[k+1]; y++ ){
				ofile_n << std::scientific << n_z_[k][y][j] << ' ';
			}
			ofile_n << std::endl;
		}
		ofile_n << std::endl;
	}

	// output responsibilities r[n][i]
	std::string opath_r = opath + ".CGSposterior";
	std::ofstream ofile_r( opath_r.c_str() );
	for( int n = 0; n < Global::posSequenceSet->getN(); n++ ){
		for( int i = 0; i < posSeqs[n]->getL()-W+1; i++ ){
			ofile_r << std::scientific << r_[n][i] << ' ';
		}
		ofile_r << std::endl;
	}

	// output parameter alphas alpha[k][j]
	std::string opath_alpha = opath + ".CGSalpha";
	std::ofstream ofile_alpha( opath_alpha.c_str() );
	for( int k = 0; k < Global::modelOrder+1; k++ ){
		ofile_alpha << "k = " << k << std::endl;
		for( int j = 0; j < W; j++ ){
			ofile_alpha << std::setprecision( 3 ) << alpha_[k][j] << ' ';
		}
		ofile_alpha << std::endl;
	}

}

