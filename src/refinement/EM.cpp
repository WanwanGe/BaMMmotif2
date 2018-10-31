//
// Created by wanwan on 16.08.17.
//
#include "EM.h"
#include <chrono>
#include "../dependencies/LBFGS/LBFGS.h"

using Eigen::VectorXd;
using namespace LBFGSpp;

EM::EM( Motif* motif, BackgroundModel* bgModel,
        std::vector<Sequence*> seqs, bool optimizeQ, bool optimizePos, bool verbose, float f ){

    motif_      = motif;
	bgModel_    = bgModel;
	q_          = motif->getQ();    // get fractional prior q from initial motifs
    f_          = f;
	seqs_       = seqs;
    optimizeQ_  = optimizeQ;
    optimizePos_= optimizePos;
    beta_       = 5;                // a hyper-parameter for estimating the positional prior
    beta2_      = 400;              // for method 2 and 3

    // get motif (hyper-)parameters from motif class
	K_ = motif_->getK();
	W_ = motif_->getW();
	Y_ = motif_->getY();
	s_ = motif_->getS();
	A_ = motif_->getA();
	K_bg_ = ( bgModel_->getOrder() < K_ ) ? bgModel_->getOrder() : K_;

	// allocate memory for r_[n][i], pos_[n][i], z_[n]
	r_ = ( float** )calloc( seqs_.size(), sizeof( float* ) );
	pos_ = ( float** )calloc( seqs_.size(), sizeof( float* ) );
	for( size_t n = 0; n < seqs_.size(); n++ ){
		r_[n] = ( float* )calloc( seqs_[n]->getL()+W_+1, sizeof( float ) );
		pos_[n] = ( float* )calloc( seqs_[n]->getL()+1, sizeof( float ) );
	}

	// allocate memory for n_[k][y][j]
	n_ = ( float*** )calloc( K_+1, sizeof( float** ) );
	for( size_t k = 0; k < K_+1; k++ ){
		n_[k] = ( float** )calloc( Y_[k+1], sizeof( float* ) );
		for( size_t y = 0; y < Y_[k+1]; y++ ){
			n_[k][y] = ( float* )calloc( W_, sizeof( float ) );
		}
	}

    // allocate memory for pi_[i]
    pi_ = ( float* )calloc( seqs_[0]->getL()-W_+2, sizeof(float) );
    b_vector_ = Eigen::VectorXf::Zero( seqs_[0]->getL()-W_+1 );
    si_ = Eigen::VectorXf::Zero( seqs_[0]->getL()-W_+1 );
    rngx_.seed( 42 );

    verbose_ = verbose;
}

EM::~EM(){
    for( size_t n = 0; n < seqs_.size(); n++ ){
        free( r_[n] );
        free( pos_[n] );
    }
    free( r_ );
    free( pos_ );

    for( size_t k = 0; k < K_+1; k++ ){
        for( size_t y = 0; y < Y_[k+1]; y++ ){
            free( n_[k][y] );
        }
        free( n_[k] );
    }
    free( n_ );

    free( pi_ );
}

int EM::optimize(){

    auto    t0_wall = std::chrono::high_resolution_clock::now();

    bool 	iterate = true;

    float 	v_diff, llikelihood_prev, llikelihood_diff;

    std::vector<std::vector<float>> v_before;
    v_before.resize( Y_[K_+1] );
    for( size_t y = 0; y < Y_[K_+1]; y++ ){
        v_before[y].resize( W_ );
    }

    // initialized positional priors
    initializePos();

    // pre-define hyper-parameters
    size_t L = seqs_[0]->getL();
    size_t LW1 = L - W_+1;
//    size_t posN = seqs_.size();
//    // calculate the value N0 and Ni_i:
//    float N_0 = 0.f;
//    for (size_t n = 0; n < posN; n++) {
//        N_0 += r_[n][0];
//    }
//    std::vector<float> N_i( LW1+1, 0.f );
//    for( size_t i = 1; i <= LW1; i++ ) {
//        for (size_t n = 0; n < posN; n++) {
//            N_i[i] += r_[n][L - i];
//        }
//    }

    norm_ = 0.0f;
    for( size_t i = 1; i <= LW1; i++ ) {
        std::normal_distribution<> nd{ LW1/2.0f, 2.0f * W_ };
        si_[i-1] = nd( rngx_ ) / LW1;
        pi_[i] = expf( si_[i-1] );
        norm_ += pi_[i];
    }
    for( size_t i = 1; i <= LW1; i++ ) {
        pi_[i] /= norm_;
        si_[i-1] = logf( pi_[i] );
    }

    // ========================
    // todo: write out for checking:
    std::string opath_pi = "/home/wanwan/benchmark/art/toyset/pi_orig.txt";
    std::string opath_si = "/home/wanwan/benchmark/art/toyset/si_orig.txt";

    std::ofstream ofile_pi( opath_pi );
    std::ofstream ofile_si( opath_si );

    for( size_t i = 1; i <= LW1; i++ ) {
        ofile_pi << pi_[i] << std::endl;
        ofile_si << si_[i-1] << std::endl;
    }
    // ========================

    // define matrix A:
    Eigen::MatrixXf A_matrix = Eigen::MatrixXf::Zero( LW1, LW1 );
    for( size_t i = 0; i < LW1; i++ ){
        for( size_t j = 0; j < LW1; j++ ){
            if( i == j ) {
                A_matrix(i, j) = 2;
            } else if( abs( i-j ) == 1 ){
                A_matrix(i, j) = -1;
            }
        }
    }
    A_matrix(0, 0) = 1;
    A_matrix(LW1-1, LW1-1) = 1;

    // iterate over
    size_t iteration = 0;
    while( iterate && ( iteration < maxEMIterations_ ) ){

        iteration++;

        // get parameter variables with highest order before EM
        llikelihood_prev = llikelihood_;

        for( size_t y = 0; y < Y_[K_+1]; y++ ){
            for( size_t j = 0; j < W_; j++ ){
                v_before[y][j] = motif_->getV()[K_][y][j];
            }
        }

        // E-step: calculate posterior
        EStep();

        // M-step: update model parameters
        MStep();

        // optimize hyper-parameter q in the first 5 steps
        if( optimizeQ_ and iteration <= 10 ) {
            optimizeQ();
        }

        // optimize positional prior pos_
        // note: this only works for sequences with the same length
        if( optimizePos_ and iteration < 2 ){
            for( size_t it = 0; it < 5; it++ ){
                optimizePos( A_matrix );
            }
        }

        // check parameter difference for convergence
        v_diff = 0.0f;
        for( size_t y = 0; y < Y_[K_+1]; y++ ){
            for( size_t j = 0; j < W_; j++ ){
                v_diff += fabsf( motif_->getV()[K_][y][j] - v_before[y][j] );
            }
        }

        // check the change of likelihood for convergence
        llikelihood_diff = llikelihood_ - llikelihood_prev;

        if( verbose_ ) std::cout << iteration << " iter, llh=" << llikelihood_
                                 << ", diff_llh=" << llikelihood_diff
                                 << ", v_diff=" << v_diff
                                 << std::endl;

        if( v_diff < epsilon_ )							iterate = false;
        if( iteration > 10 )							iterate = false;
        //if( llikelihood_diff < 0 and iteration > 10 )	iterate = false;

/*
        // for making a movie out of all iterations
        // calculate probabilities
        motif_->calculateP();
        // write out the learned model
        motif_->write( odir, filename + std::to_string( iteration ) );
*/

    }

    // calculate probabilities
    motif_->calculateP();
    auto t1_wall = std::chrono::high_resolution_clock::now();
    auto t_diff = std::chrono::duration_cast<std::chrono::duration<double>>(t1_wall-t0_wall);
    std::cout << "\n--- Runtime for EM: " << t_diff.count() << " seconds ---\n";

    return 0;
}

void EM::EStep(){

    float llikelihood = 0.0f;

    motif_->calculateLinearS( bgModel_->getV(), K_bg_ );

    // calculate responsibilities r at all LW1 positions on sequence n
    // n runs over all sequences
#pragma omp parallel for reduction(+:llikelihood)
    for( size_t n = 0; n < seqs_.size(); n++ ){

        size_t 	L = seqs_[n]->getL();
        size_t*	kmer = seqs_[n]->getKmer();
        float 	normFactor = 0.f;

        // initialize r_[n][i] and pos_[n][i]:
        // note here:   r_[n][0] and pos_[n][0] are for motif is absent
        //              and the indices of r_[n][i] is reverted, which means:
        //              r_[n][i] is the weight for motif being at position L-i on sequence n
        //              the purpose of doing the reverting is to improve the computation speed

        for( size_t i = 0; i <= L+W_; i++ ){
            r_[n][i] = 1.0f;
        }

        // when p(z_n > 0), ij = i+j runs over all positions in sequence
        // r index goes from (0, L]
        for( size_t ij = 0; ij < L; ij++ ){

            // extract (K+1)-mer y from positions (ij-K,...,ij)
            size_t y = kmer[ij] % Y_[K_+1];

            for( size_t j = 0; j < W_; j++ ){
                r_[n][L-ij+j] *= s_[y][j];
            }
        }

        // calculate the responsibilities and sum them up
        r_[n][0] = pos_[n][0];
        normFactor += r_[n][0];
        for( size_t i = W_-1; i <= L-1; i++ ){
            r_[n][i] *= pos_[n][L-i];
            normFactor += r_[n][i];
        }

        // normalize responsibilities
        for( size_t i = W_-1; i <= L-1; i++ ){
            r_[n][i] /= normFactor;
        }

        // for the unused positions
        for( size_t i = 1; i <= W_-2; i++ ){
            r_[n][i] = 0.0f;
        }
        for( size_t i = L; i <= L+W_; i++ ){
            r_[n][i] = 0.0f;
        }

        // calculate log likelihood over all sequences
        llikelihood += logf( normFactor );
    }

    llikelihood_ = llikelihood;

}

void EM::MStep(){

    // reset the fractional counts n
    for( size_t k = 0; k < K_+1; k++ ){
        for( size_t y = 0; y < Y_[k+1]; y++ ){
            for( size_t j = 0; j < W_; j++ ){
                n_[k][y][j] = 0.0f;
            }
        }
    }

    // compute fractional occurrence counts for the highest order K
    // n runs over all sequences
#pragma omp parallel for
    for( size_t n = 0; n < seqs_.size(); n++ ){
        size_t  L = seqs_[n]->getL();
        size_t* kmer = seqs_[n]->getKmer();

        // ij = i+j runs over all positions i on sequence n
        // r index goes from (0, L]
        for( size_t ij = 0; ij < L; ij++ ){
            size_t y = kmer[ij] % Y_[K_+1];
            for (size_t j = 0; j < W_; j++) {
                // parallelize for: n_[K_][y][j] += r_[n][L-ij+j];
                atomic_float_add(&(n_[K_][y][j]), r_[n][L-ij+j]);
            }
        }
    }

    // compute fractional occurrence counts from higher to lower order
    // k runs over all lower orders
    for( size_t k = K_; k > 0; k-- ){
        for( size_t y = 0; y < Y_[k+1]; y++ ){
            size_t y2 = y % Y_[k];
            for( size_t j = 0; j < W_; j++ ){
                n_[k-1][y2][j] += n_[k][y][j];
            }
        }
    }

    //printR();
    // update model parameters v[k][y][j] with updated k-mer counts, alphas and model order
    motif_->updateV( n_, A_, K_ );
}

void EM::initializePos(){

    for( size_t n = 0; n < seqs_.size(); n++ ){
        size_t 	L = seqs_[n]->getL();
        size_t 	LW1 = L - W_ + 1;
        pos_[n][0] = 1.0f - q_;                         // probability of having no motif on the sequence
        float   pos_i = q_ / static_cast<float>( LW1 ); // probability of having a motif at position i on the sequence
        for( size_t i = 1; i <= L; i++ ){
            pos_[n][i] = pos_i;
        }
    }
    
}

void EM::EStep_slow(){
    /**
     * together with MStep_slow(), this is the slow version of EM
     */

    float llikelihood = 0.0f;

    motif_->calculateLinearS( bgModel_->getV(), K_bg_ );

    // calculate responsibilities r at all LW1 positions on sequence n
    // n runs over all sequences
#pragma omp parallel for reduction(+:llikelihood)
    for( size_t n = 0; n < seqs_.size(); n++ ){

        size_t 	L = seqs_[n]->getL();
        size_t 	LW1 = L - W_ + 1;
        size_t*	kmer = seqs_[n]->getKmer();
        float 	normFactor = 0.f;

        // initialize r_[n][i] with 1:
        // note here:   r_[n][0] is the responsibility when motif is absent
        //              and the indices of r_[n][i] is reverted, which means:
        //              r_[n][i] is the weight for motif being at position i on sequence n
        for( size_t i = 0; i <= LW1; i++ ){
            r_[n][i] = 1.0f;
        }

        // when p(z_n > 0), it runs over all positions in sequence in the region of [1, LW1]
        for( size_t i = 1; i <= LW1; i++ ){
            for( size_t j = 0; j < W_; j++ ){
                // extract (K+1)-mer y from positions (ij-K,...,ij)
                size_t y = kmer[i+j-1] % Y_[K_+1];
                r_[n][i] *= s_[y][j];
            }
        }

        // calculate the responsibilities and sum them up
        for( size_t i = 0; i <= LW1; i++ ){
            r_[n][i] *= pos_[n][i];
            normFactor += r_[n][i];
        }

        // normalize responsibilities in the region [0, LW1]
        for( size_t i = 0; i <= LW1; i++ ){
            r_[n][i] /= normFactor;
        }

        // calculate log likelihood over all sequences
        llikelihood += logf( normFactor );
    }

    llikelihood_ = llikelihood;

}

void EM::MStep_slow(){
    /**
     * together with EStep_slow(), this is the slow version of EM
     */

    // reset the fractional counts n
    for( size_t k = 0; k < K_+1; k++ ){
        for( size_t y = 0; y < Y_[k+1]; y++ ){
            for( size_t j = 0; j < W_; j++ ){
                n_[k][y][j] = 0.0f;
            }
        }
    }

    // compute fractional occurrence counts for the highest order K
    // n runs over all sequences
#pragma omp parallel for
    for( size_t n = 0; n < seqs_.size(); n++ ){
        size_t  L = seqs_[n]->getL();
        size_t  LW1 = L - W_ + 1;
        size_t* kmer = seqs_[n]->getKmer();

        // it runs over all positions i on sequence n in the region of [1, LW1]
        for( size_t i = 1; i <= LW1; i++ ){
            for (size_t j = 0; j < W_; j++) {
                size_t y = kmer[i+j-1] % Y_[K_+1];
                //n_[K_][y][j] += r_[n][i];
                atomic_float_add(&(n_[K_][y][j]), r_[n][i]);
            }
        }
    }

    // compute fractional occurrence counts from higher to lower order
    // k runs over all lower orders
    for( size_t k = K_; k > 0; k-- ){
        for( size_t y = 0; y < Y_[k+1]; y++ ){
            size_t y2 = y % Y_[k];
            for( size_t j = 0; j < W_; j++ ){
                n_[k-1][y2][j] += n_[k][y][j];
            }
        }
    }

    //printR();
    // update model parameters v[k][y][j] with updated k-mer counts, alphas and model order
    motif_->updateV( n_, A_, K_ );
}

int EM::mask() {
    /**
     * upgraded version of EM to eliminate the effect of unrelated motifs
     */
    auto t0_wall = std::chrono::high_resolution_clock::now();

    /**
     * E-step for k = 0 to estimate weights r
     */
    // calculate the log odds ratio for k=0
    for( size_t y = 0; y < Y_[1]; y++ ){
        for( size_t j = 0; j < W_; j++ ){
            s_[y][j] = motif_->getV()[0][y][j] / bgModel_->getV()[0][y];
        }
    }

    // initialized positional priors
    initializePos();
    size_t posN = seqs_.size();
    std::vector<float> r_all;   // add all r's to an array for sorting
    size_t N_position = 0;      // count the number of all possible positions

    // calculate responsibilities r at all LW1 positions on sequence n
    // n runs over all sequences
//#pragma omp parallel for
    for( size_t n = 0; n < posN; n++ ) {

        size_t  L = seqs_[n]->getL();
        size_t  *kmer = seqs_[n]->getKmer();

        // initialize r_[n][i]
        for (size_t i = 0; i <= L+W_; i++) {
            r_[n][i] = 1.0f;
        }

        // when p(z_n > 0), ij = i+j runs over all positions in sequence
        // r index goes from (0, L]
        for (size_t ij = 0; ij < L; ij++) {

            // extract monomer y at position i
            size_t y = kmer[ij] % Y_[1];

            for( size_t j = 0; j < W_; j++ ){
                r_[n][L-ij+j] *= s_[y][j];
            }
        }

        // calculate the responsibilities and sum them up
        r_[n][0] = pos_[n][0];
        for( size_t i = W_-1; i <= L-1; i++ ){
            r_[n][i] *= pos_[n][L-i];
        }

        // add all un-normalized r to a vector
        r_all.push_back( r_[n][0] );
        for( size_t i = W_-1; i <= L-1; i++ ){
            r_all.push_back( r_[n][i] );
        }

        // count the number of all possible motif positions
        N_position += L-W_+1;
    }

    /**
     * found the cutoff of responsibility that covers the top f_% of the motif occurrences
     * by applying partition-based selection algorithm, which runs in O(n)
     */
    // Sort log odds scores in descending order
    std::sort( r_all.begin(), r_all.end(), std::greater<float>() );

    // find the cutoff with f_% best r's
    float r_cutoff = r_all[size_t( N_position * f_ )];

    // create a vector to store all the r's which are above the cutoff
    std::vector<std::vector<size_t>> ridx( posN );

    // put the index of the f_% r's in an array
    for( size_t n = 0; n < posN; n++ ){
        for( size_t i = seqs_[n]->getL() - 1; i >= W_-1; i-- ){
            if( r_[n][i] >= r_cutoff ){
                ridx[n].push_back( i );
            }
        }
    }

    /**
     * optimize the model with the highest order from the top f_% global occurrences
     * using EM till convergence
     */
    bool 	iterate = true;
    size_t  iteration = 0;
    float 	v_diff, llikelihood_prev, llikelihood_diff;

    std::vector<std::vector<float>> v_before;
    v_before.resize( Y_[K_+1] );
    for( size_t y = 0; y < Y_[K_+1]; y++ ){
        v_before[y].resize( W_ );
    }

    // iterate over
    while( iterate && ( iteration < maxEMIterations_ ) ){

        iteration++;

        // get parameter variables with highest order before EM
        llikelihood_prev = llikelihood_;
        for( size_t y = 0; y < Y_[K_+1]; y++ ){
            for( size_t j = 0; j < W_; j++ ){
                v_before[y][j] = motif_->getV()[K_][y][j];
            }
        }

        /**
         * E-step for f_% motif occurrences
         */
        float llikelihood = 0.0f;

        motif_->calculateLinearS( bgModel_->getV(), K_bg_ );

        // calculate responsibilities r at all LW1 positions on sequence n
        // n runs over all sequences
        for( size_t n = 0; n < seqs_.size(); n++ ){

            size_t 	L = seqs_[n]->getL();
            size_t*	kmer = seqs_[n]->getKmer();
            float 	normFactor = 0.f;

            // initialize all filtered r_[n][i]
            for( size_t idx = 0; idx < ridx[n].size(); idx++ ){
                r_[n][ridx[n][idx]] = 1.0f;
            }

            // update r based on the log odd ratios
            for( size_t idx = 0; idx < ridx[n].size(); idx++ ){
                for( size_t j = 0; j < W_; j++ ){
                    size_t y = kmer[L-ridx[n][idx]+j] % Y_[K_+1];
                    r_[n][ridx[n][idx]] *= s_[y][j];
                }
                // calculate the responsibilities and sum them up
                r_[n][ridx[n][idx]] *= pos_[n][L-ridx[n][idx]];
                normFactor += r_[n][ridx[n][idx]];
            }

            // normalize responsibilities
            normFactor += r_[n][0];
            r_[n][0] /= normFactor;
            for( size_t idx = 0; idx < ridx[n].size(); idx++ ){
                r_[n][ridx[n][idx]] /= normFactor;
            }

            // calculate log likelihood over all sequences
            llikelihood += logf( normFactor );
        }

        llikelihood_ = llikelihood;
        /**
         * M-step for f_% motif occurrences
         */
        // reset the fractional counts n
        for( size_t k = 0; k < K_+1; k++ ){
            for( size_t y = 0; y < Y_[k+1]; y++ ){
                for( size_t j = 0; j < W_; j++ ){
                    n_[k][y][j] = 0.0f;
                }
            }
        }

        // compute fractional occurrence counts for the highest order K
        // using the f_% r's
        // n runs over all sequences
        for( size_t n = 0; n < posN; n++ ){
            size_t  L = seqs_[n]->getL();
            size_t* kmer = seqs_[n]->getKmer();

            for( size_t idx = 0; idx < ridx[n].size(); idx++ ){
                for( size_t j = 0; j < W_; j++ ){
                    size_t y = kmer[L-ridx[n][idx]+j] % Y_[K_+1];
                    n_[K_][y][j] += r_[n][ridx[n][idx]];
                }
            }
        }

        // compute fractional occurrence counts from higher to lower order
        // k runs over all orders
        for( size_t k = K_; k > 0; k-- ){
            for( size_t y = 0; y < Y_[k+1]; y++ ){
                size_t y2 = y % Y_[k];
                for( size_t j = 0; j < W_; j++ ){
                    n_[k-1][y2][j] += n_[k][y][j];
                }
            }
        }

        // update model parameters v[k][y][j], due to the k-mer counts, alphas and model order
        motif_->updateV( n_, A_, K_ );

        /**
         * check parameter difference for convergence
         */
        v_diff = 0.0f;
        for( size_t y = 0; y < Y_[K_+1]; y++ ){
            for( size_t j = 0; j < W_; j++ ){
                v_diff += fabsf( motif_->getV()[K_][y][j] - v_before[y][j] );
            }
        }

        // check the change of likelihood for convergence
        llikelihood_diff = llikelihood_ - llikelihood_prev;
        if( verbose_ ) std::cout << iteration << " iter, llh=" << llikelihood_
                                 << ", diff_llh=" << llikelihood_diff
                                 << ", v_diff=" << v_diff
                                 << std::endl;
        if( v_diff < epsilon_ )							iterate = false;
        if( llikelihood_diff < 0 and iteration > 10 )	iterate = false;
    }

    // calculate probabilities
    motif_->calculateP();

    auto t1_wall = std::chrono::high_resolution_clock::now();
    auto t_diff = std::chrono::duration_cast<std::chrono::duration<double>>(t1_wall-t0_wall);
    std::cout << "\n--- Runtime for EM: " << t_diff.count() << " seconds ---\n";

    return 0;
}

void EM::optimizeQ(){

    float N_i = 0.f;                   // expectation value of the count of sequences with a query motif
    size_t posN = seqs_.size();

    for( size_t n = 0; n < posN; n++ ){
        for( size_t i = W_-1; i <= seqs_[n]->getL()-1; i++ ){
            N_i += r_[n][i];
        }
    }

    q_ = ( N_i + 1.f ) / ( posN + 2.f );

    if( verbose_ ) std::cout << "optimized q=" << q_ << std::endl;

}

void EM::optimizePos( Eigen::MatrixXf A_matrix ) {

    // todo: note this function currently only works for sequences of the same length
    size_t L = seqs_[0]->getL();
    size_t LW1 = L - W_ + 1;
    size_t LW2 = L - W_ + 2;
    size_t posN = seqs_.size();

    // update positional prior using the smoothness parameter beta
    // according to Eq. 149
    // calculate the value N0 and Ni_i:
    float N_0 = 0.f;
    for (size_t n = 0; n < posN; n++) {
        N_0 += r_[n][0];
    }
    std::vector<float> N_i( LW2, 0.f );
    for( size_t i = 1; i <= LW1; i++ ) {
        for (size_t n = 0; n < posN; n++) {
            N_i[i] += r_[n][L - i];
        }
    }

    size_t method_flag = 2;

    if( method_flag == 1 ){

        // method 1: Using a flat Bayesian prior on positional preference
        // according to Eq. 149
        for( size_t i = 1; i <= LW1; i++ ) {
            // update pi according to Eq. 149
            pi_[i] = (N_i[i] + beta_ - 1.f) / (posN - N_0 + LW1 * (beta_ - 1.f));
        }

        // additional check:
        float sum = 0.f;
        for (size_t i = 1; i < LW1; i++) {
            sum += powf( logf( pi_[i+1] ) - logf( pi_[i] ), 2.f );
        }
        std::cout << "sum=" << sum << ", beta=" << LW2 / sum << std::endl;

    } else if ( method_flag == 2 ){

        // method 2: Using a prior for penalising jumps in the positional preference profile
        // run a few iterations of conjugate gradients (e.g. 5~10)
        for( size_t i = 1; i <= LW1; i++ ) {
//            b_vector_[i - 1] = ( N_i[i] - (posN - N_0) * expf( si_[i-1] ) / norm_ ) / beta2_;
            b_vector_[i - 1] = ( N_i[i] - (posN - N_0) * pi_[i] ) / beta2_;
        }

        Eigen::ConjugateGradient<Eigen::MatrixXf, Eigen::Lower | Eigen::Upper> cg;
        cg.compute( A_matrix );
        cg.setMaxIterations( 1 );
        si_ = cg.solve( b_vector_ );

/*
        // method 3: use LBFGS optimizer instead of CG
        // set up parameters
        LBFGSParam<float> param;
        param.epsilon = 1e-6f;
        param.max_iterations = 100;

        // create colver and function object
        LBFGSpp::LBFGSSolver<float> solver( param );
        func = ;

        // solve the function
        float fx;
        int niter = solver.minimize( func, si_, fx);

*/

        // get pi from si after normalization
        norm_ = 0.f;
        float norm_si = 0.f;
        for( size_t i = 1; i <= LW1; i++ ) {
            pi_[i] = expf( si_[i-1] );
            norm_ += pi_[i];
            norm_si += si_[i-1];
        }
        for( size_t i = 1; i <= LW1; i++ ) {
            pi_[i] /= norm_;
            // normalization of si
            si_[i-1] -= norm_si / LW1;

        }

        // ========================
        // todo: write out for checking:
        std::string opath_pos = "/home/wanwan/benchmark/art/toyset/pos.txt";
        std::string opath_pi = "/home/wanwan/benchmark/art/toyset/pi.txt";
        std::string opath_si = "/home/wanwan/benchmark/art/toyset/si.txt";
        std::string opath_bv = "/home/wanwan/benchmark/art/toyset/bv.txt";
        std::string opath_left = "/home/wanwan/benchmark/art/toyset/left.txt";
        std::string opath_middle = "/home/wanwan/benchmark/art/toyset/middle.txt";
        std::string opath_right = "/home/wanwan/benchmark/art/toyset/right.txt";
        std::string opath_grad = "/home/wanwan/benchmark/art/toyset/grad.txt";
        std::string opath_smooth = "/home/wanwan/benchmark/art/toyset/smooth.txt";

        std::ofstream ofile_pos( opath_pos );
        std::ofstream ofile_pi( opath_pi );
        std::ofstream ofile_si( opath_si );
        std::ofstream ofile_bv( opath_bv );
        std::ofstream ofile_left( opath_left );
        std::ofstream ofile_middle( opath_middle );
        std::ofstream ofile_right( opath_right );
        std::ofstream ofile_grad( opath_grad );
        std::ofstream ofile_smooth( opath_smooth );

        Eigen::VectorXf right_vector =  beta2_ * ( A_matrix * si_ );

        for( size_t i = 1; i <= LW1; i++ ) {
            ofile_pos << pi_[i] * q_ << std::endl;
            ofile_pi << pi_[i] << std::endl;
            ofile_si << si_[i-1] << std::endl;
            ofile_bv << b_vector_[i-1] << std::endl;
            ofile_left << N_i[i] << std::endl;
            float middle = (posN - N_0) * pi_[i];
            ofile_middle << middle << std::endl;
            ofile_right << right_vector[i-1] << std::endl;
            ofile_grad << N_i[i] - middle - right_vector[i-1] << std::endl;
            ofile_smooth << ( N_i[i] - right_vector[i-1] ) / (posN - N_0) << std::endl;
        }
        // ========================


        // method 3: update smoothness parameter beta using positional prior distribution
        // from the data
        // according to Eq. 158, note the indices for si
        float sum = 0.f;
        for (size_t i = 1; i < LW1; i++) {
            sum += powf( si_[i] - si_[i-1], 2.f );
            //sum += powf( logf(pi_[i+1]) - logf(pi_[i]), 2.f );
        }

        // update beta by its expectation value
        //beta2_ = LW2 / sum;

        std::cout << "N0=" << N_0 << ", N=" << posN
                  << ", LW1=" << LW1 << ",  norm=" << norm_
                  << ", sum=" << sum << ", beta=" << LW2 / sum << std::endl;

    } else if( method_flag == 3 ){
        // prior penalising kinks in the positional preference profile
        // define matrix B:
        std::vector< std::vector<int>> B_matrix(LW1, std::vector<int> ( LW1, 0 ));
        for( size_t i = 0; i < LW1; i++ ){
            for( size_t j = 0; j < LW1; j++ ){
                if( i == j ) {
                    B_matrix[i][j] = 6;
                } else if( std::abs( i-j ) == 1 ){
                    B_matrix[i][j] = -4;
                } else if( std::abs( i-j ) == 2 ){
                    B_matrix[i][j] = 1;
                }
            }
        }
        B_matrix[0][0] = B_matrix[LW1-1][LW1-1] = 1;
        B_matrix[1][1] = B_matrix[LW1-2][LW1-2] = 5;
        B_matrix[0][1] = B_matrix[1][0] = B_matrix[LW1-1][LW1-2] = B_matrix[LW1-2][LW1-1] = -2;
    }

    // update pos_ni by pi_i
    for( size_t i = 1; i <= LW1; i++ ) {
        for (size_t n = 0; n < posN; n++) {
            pos_[n][i] = pi_[i] * q_;
        }
    }

}

float** EM::getR(){
    return r_;
}

float EM::getQ(){
    return q_;
}

void EM::print(){

    // print out motif parameter v
    for( size_t j = 0; j < W_; j++ ){
        for( size_t y = 0; y < Y_[K_+1]; y++ ){
            std::cout << std::setprecision( 3 ) << n_[K_][y][j] << '\t';
        }
        std::cout << std::endl;
    }

}

void EM::printR(){

    // print out weights r for each position on each sequence
    for( size_t n = 0; n < seqs_.size(); n++ ){
        std::cout << "seq " << n << ":" << std::endl;
        for( size_t i = 0; i < seqs_[n]->getL(); i++ ){
            std::cout << r_[n][seqs_[n]->getL()-W_-i] << '\t';
        }
        std::cout << std::endl;
    }
}

void EM::write( char* odir, std::string basename, bool ss ){

	/**
	 * 	 * save BaMM (hyper-)parameters in flat files:
	 * (1) posSequenceBasename.counts:	    refined fractional counts of (k+1)-mers
	 * (2) posSequenceBasename.weights:     responsibilities, posterior distributions
	 * (3) posSequenceBasename.positions:   position of motif(s) on each sequence
	 */

	std::string opath = std::string( odir ) + '/' + basename;

	// output (k+1)-mer counts n[k][y][j]
	std::string opath_n = opath + ".counts";
	std::ofstream ofile_n( opath_n.c_str() );
	for( size_t j = 0; j < W_; j++ ){
		for( size_t k = 0; k < K_+1; k++ ){
			for( size_t y = 0; y < Y_[k+1]; y++ ){
				ofile_n << static_cast<int>( n_[k][y][j] ) << '\t';
			}
			ofile_n << std::endl;
		}
		ofile_n << std::endl;
	}

	// output position(s) of motif(s): pos_[n][i]
	std::string opath_pos = opath + ".positions";
	std::ofstream ofile_pos( opath_pos.c_str() );

	ofile_pos << "seq\tlength\tstrand\tstart..end\tpattern" << std::endl;

    float cutoff = 0.3f;	// threshold for having a motif on the sequence
                            // in terms of responsibilities
    for( size_t n = 0; n < seqs_.size(); n++ ){

        size_t L = seqs_[n]->getL();
        L = ss ? L : ( L - 1 ) / 2;

        for( size_t i = 0; i < seqs_[n]->getL()-W_+1; i++ ){

            if( r_[n][seqs_[n]->getL() -W_-i] >= cutoff ){
                ofile_pos << seqs_[n]->getHeader() << '\t' << L << '\t'
                          << ( ( i < L ) ? '+' : '-' ) << '\t' << i + 1 << ".." << i+W_ << '\t';
                for( size_t b = i; b < i+W_; b++ ){
                    ofile_pos << Alphabet::getBase( seqs_[n]->getSequence()[b] );
                }
                ofile_pos << std::endl;
            }
        }
    }

	// output responsibilities r[n][i]
	bool write_r = false;		// flag for writing out the responsibilities
	if( write_r ){
		std::string opath_r = opath + ".weights";
		std::ofstream ofile_r( opath_r.c_str() );
		for( size_t n = 0; n < seqs_.size(); n++ ){
            ofile_r << std::setprecision( 2 ) << r_[n][0] << ' ';
			for( size_t i = W_+1; i < seqs_[n]->getL(); i++ ){
				ofile_r << std::setprecision( 2 ) << r_[n][seqs_[n]->getL()-i] << ' ';
			}
			ofile_r << std::endl;
		}
	}
}