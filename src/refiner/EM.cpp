//
// Created by wanwan on 16.08.17.
//
#include "EM.h"

EM::EM( Motif* motif, BackgroundModel* bgModel,
        std::vector<Sequence*> seqs/*, bool singleStrand,
        bool optimizeQ,
        bool optimizePos, bool verbose, float f */){

    motif_      = motif;
	bgModel_    = bgModel;
	q_          = motif->getQ();    // get fractional prior q from initial motifs
	seqs_       = seqs;
    beta1_      = 5;                // a hyper-parameter for estimating the positional prior
    beta2_      = 400;              // for method 2 and 3

    // get motif (hyper-)parameters from motif class
	K_ = Global::modelOrder;
	W_ = motif_->getW();
	Y_ = motif_->getY();
	s_ = motif_->getS();
	A_ = motif_->getA();
    // todo: this introduces a bug when optimizing the 0th-order model
//	K_bg_ = ( bgModel_->getOrder() < K_ ) ? bgModel_->getOrder() : K_;
    K_bg_ = Global::bgModelOrder;

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
    N0_         = 0.f;
    LW1_        = seqs_[0]->getL()-W_+1;
    pi_         = ( float* )calloc( LW1_+1, sizeof( float ) );

//    b_vector_   = Eigen::VectorXf::Zero( LW1_ );
//    si_         = Eigen::VectorXf::Zero( LW1_ );
//    Ni_         = Eigen::VectorXf::Zero( LW1_ );
//    A_matrix_   = getAmatrix( LW1_ );

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

    if( Global::verbose ) {
        std::cout << " ______" << std::endl;
        std::cout << "|*    *|" << std::endl;
        std::cout << "|  EM  |" << std::endl;
        std::cout << "|*____*|" << std::endl;
        std::cout << std::endl;
    }

    auto    t0_wall = std::chrono::high_resolution_clock::now();

    bool 	iterate = true;

    float 	v_diff, llikelihood_prev, llikelihood_diff;

    // store model probs before each optimization step
    std::vector<std::vector<float>> v_before;
    v_before.resize( Y_[K_+1] );
    for( size_t y = 0; y < Y_[K_+1]; y++ ){
        v_before[y].resize( W_ );
    }

    // initialized positional priors
    updatePos();

/*
    if( optimizePos_ ) {
        // pre-define hyper-parameters for optimizing position priors
        size_t L = seqs_[0]->getL();
        size_t LW1 = L - W_ + 1;

        norm_ = 0.0f;
        for (size_t i = 1; i <= LW1; i++) {
            std::normal_distribution<> nd{LW1 / 2.0f, (float) LW1};
            si_[i-1] = nd(rngx_) / LW1;
            pi_[i] = expf(si_[i-1]);
            norm_ += pi_[i];
        }
        for (size_t i = 1; i <= LW1; i++) {
            pi_[i] /= norm_;
            //si_[i-1] = logf( pi_[i] );
        }
    }
*/


    // iterate over
    size_t iteration = 0;
    while( iterate && ( iteration < Global::maxIterations ) ){

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

        // optimize hyper-parameter q in the first step
        if( Global::optimizeQ and iteration < 1 ) {
            optimizeQ();
            updatePos();
        }

        // optimize positional prior pos_
        // note: this only works for sequences with the same length
        if( Global::optimizePos and iteration == 1 ){
//            optimizePos();
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

        if( Global::verbose )
            std::cout << "it=" << iteration
                      << std::fixed
                      << "\tlog likelihood=" << llikelihood_
                      << "\tllh_diff=" << llikelihood_diff
                      << "\t\tmodel_diff=" << v_diff
                      << std::endl;

        if( v_diff < Global::EMepsilon )					iterate = false;
        if( llikelihood_diff < 0 and iteration > 10 )	iterate = false;

        iteration++;

        // todo: for making a movie out of all iterations
        if( Global::makeMovie ) {
            // calculate probabilities
            motif_->calculateP();
            // write out the learned model
            std::string opath = std::string( Global::outputDirectory ) + "/movie_factory" ;
            std::string filename = "movie_clap";
            char *odir = new char[opath.length() + 1];
            std::strcpy(odir, opath.c_str());
            motif_->write(odir, filename + std::to_string(iteration));
        }
        // todo: print out for checking
        if( Global::debugMode ) {
            print();
            printR();
        }
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
        size_t  LW1 = L - W_ + 1;
        size_t*	kmer = seqs_[n]->getKmer();
        float 	normFactor = 0.f;

        // initialize r_[n][i] and pos_[n][i]:
        // note here:   r_[n][0] and pos_[n][0] are for motif is absent
        //              and the indices of r_[n][i] is reverted, which means:
        //              r_[n][i] is the weight for motif being at position L-i on sequence n
        //              the purpose of doing the reverting is to improve the computation speed

        for( size_t i = 1; i <= LW1; i++ ){
            r_[n][L-i] = 1.0f;
        }

        // when p(z_n > 0), ij = i+j runs over all positions in sequence
        // r index goes from (0, L]

        for( size_t ij = 0; ij < L; ij++ ){
            // extract (K+1)-mer y from positions (ij-K,...,ij)
            size_t y = kmer[ij] % Y_[K_+1];
            for( size_t j = 0; j < W_; j++ ){
                r_[n][L-ij+j-1] *= s_[y][j];
            }
        }

        // calculate the responsibilities and sum them up
        normFactor += pos_[n][0];
        for( size_t i = 1; i <= LW1; i++ ){
            r_[n][L-i] *= pos_[n][i];
            normFactor += r_[n][L-i];
        }

        // normalize responsibilities
        for( size_t i = 1; i <= LW1; i++ ){
            r_[n][L-i] /= normFactor;
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
                // parallelize for: n_[K_][y][j] += r_[n][L-ij+j-1];
                atomic_float_add(&(n_[K_][y][j]), r_[n][L-ij+j-1]);
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

    // update model parameters v[k][y][j] with updated k-mer counts, alphas and model order
    motif_->updateV( n_, A_, K_ );
}

void EM::updatePos(){

    for( size_t n = 0; n < seqs_.size(); n++ ){
        size_t 	L = seqs_[n]->getL();
        size_t 	LW1 = L - W_ + 1;
        pos_[n][0] = 1.0f - q_;                         // probability of having no motif on the sequence
        float pos_i = q_ / static_cast<float>( LW1 );   // probability of having a motif at position i on the sequence
        for( size_t i = 1; i <= L; i++ ){
            pos_[n][i] = pos_i;
        }
    }

}

void EM::EStep_slow(){
    /**
     * together with MStep_slow(), this is the slow version of EM
     */

    float llikelihood = 0.f;

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
                size_t y = kmer[i-1+j] % Y_[K_+1];
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
                size_t y = kmer[i-1+j] % Y_[K_+1];
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
    updatePos();
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
    float r_cutoff = r_all[size_t( N_position * Global::f )];

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
    while( iterate && ( iteration < Global::maxIterations ) ){

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
        if( Global::verbose ) std::cout << iteration << " iter, llh=" << llikelihood_
                                 << ", diff_llh=" << llikelihood_diff
                                 << ", v_diff=" << v_diff
                                 << std::endl;
        if( v_diff < Global::EMepsilon )							iterate = false;
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

    float N_0 = 0.f;
    size_t posN = seqs_.size();
    for (size_t n = 0; n < posN; n++) {
        N_0 += r_[n][0];
    }

    q_ = ( posN - N_0 + 1.f ) / ( posN + 2.f );

    if( !Global::ss ) q_ /= 2;

    if( Global::verbose ){
        std::cout << "optimized q=" << q_ << std::endl;
    }

}

/*
void EM::optimizePos() {

    // todo: note this function currently only works for sequences of the same length

    // update positional prior using the smoothness parameter beta
    // according to Eq. 149
    // calculate the value N0 and Ni_i:
    N0_ = 0.f;
    for (size_t n = 0; n < posN_; n++) {
        N0_ += r_[n][0];
    }

    for( size_t i = 0; i < LW1_; i++ ) {
        for (size_t n = 0; n < posN_; n++) {
            Ni_[i] += r_[n][seqs_[0]->getL()-i];
        }
    }

    size_t method_flag = 4;

    if( method_flag == 1 ){

        // method 1: Using a flat Bayesian prior on positional preference
        // according to Eq. 149
        for( size_t i = 0; i < LW1_; i++ ) {
            // update pi according to Eq. 149
            pi_[i+1] = (Ni_[i] + beta1_ - 1.f) / (posN_ - N0_ + LW1_ * (beta1_ - 1.f));
        }

    } else if ( method_flag == 2 ){

        // method 2: Using a prior for penalising jumps in the positional
        // preference profile
        // run a few iterations of conjugate gradients (e.g. 5~10)
        for( size_t i = 0; i < LW1_; i++ ) {
            b_vector_[i] = ( Ni_[i] - ( posN_ - N0_ ) * pi_[i+1] ) / beta2_;
        }

        Eigen::ConjugateGradient<Eigen::MatrixXf, Eigen::Lower | Eigen::Upper> cg;

        cg.compute( A_matrix_ );
        cg.setMaxIterations( 5 );
        si_ = cg.solve( b_vector_ );

        // normalize pi
        norm_ = 1.e-5f;
        for( size_t i = 1; i <= LW1_; i++ ) {
            pi_[i] = expf( si_[i-1] );
            norm_ += pi_[i];
        }
        for( size_t i = 1; i <= LW1_; i++ ) {
            pi_[i] /= norm_;
        }

        // calculate objective function:
        float Q;
        float p1 = 0.f;
        float p2 = 0.f;
        float sumexp = 0.f;
        // calculate objective function
        for (size_t i = 0; i < LW1_; i++) {
            sumexp += expf(si_[i]);
        }
        for (size_t i = 0; i < LW1_; i++) {
            p1 += Ni_[i] * (si_[i] - logf(sumexp));
        }
        for (size_t i = 2; i < LW1_; i++) {
            p2 += beta2_ * powf(si_[i] - si_[i - 1], 2.f);
        }
        Q = p1 - p2;
        std::cout << "cost function = " << Q << std::endl;
        // ======== end of objfun calculation =======
*/
/*
        // ========================
        // todo: write out for checking:
        std::string opath_pos = std::string( Global::outputDirectory ) + "/debug/pos.txt";
        std::string opath_pi = std::string( Global::outputDirectory ) + "/debug/pi.txt";
        std::string opath_bv = std::string( Global::outputDirectory ) + "/debug/bv.txt";
        std::string opath_left = std::string( Global::outputDirectory ) + "/debug/left.txt";
        std::string opath_middle = std::string( Global::outputDirectory ) + "/debug/middle.txt";
        std::string opath_right = std::string( Global::outputDirectory ) + "/debug/right.txt";
        std::string opath_grad = std::string( Global::outputDirectory ) + "/debug/grad.txt";
        std::string opath_ri = std::string( Global::outputDirectory ) + "/debug/ri.txt";

        std::ofstream ofile_pos( opath_pos );
        std::ofstream ofile_pi( opath_pi );
        std::ofstream ofile_bv( opath_bv );
        std::ofstream ofile_left( opath_left );
        std::ofstream ofile_middle( opath_middle );
        std::ofstream ofile_right( opath_right );
        std::ofstream ofile_grad( opath_grad );
        std::ofstream ofile_ri( opath_ri );

        Eigen::VectorXf right_vector = beta2_ * ( getAmatrix(LW1) * si_ );

        for( size_t i = 1; i <= LW1; i++ ) {
            ofile_pos << pi_[i] * q_ << std::endl;
            ofile_pi << pi_[i] << std::endl;
            ofile_bv << b_vector_[i-1] << std::endl;
            ofile_left << N_i[i] << std::endl;
            float middle = (posN - N_0) * pi_[i];
            ofile_middle << middle << std::endl;
            ofile_right << right_vector[i-1] << std::endl;
            ofile_grad << N_i[i] - middle - right_vector[i-1] << std::endl;
        }

        for( size_t n = 0; n < posN; n++ ){
            for( size_t i = 1; i < LW1; i++ ){
                ofile_ri << std::setprecision( 2 ) << r_[n][L-i] << '\t';
            }
            ofile_ri << r_[n][L-LW1] << std::endl;
        }
        // ========================
*//*


        // method 2b: update smoothness parameter beta using positional prior distribution
        // from the data
        // according to Eq. 158, note the indices for si
        float sum = 1.e-8f;
        for (size_t i = 1; i < LW1_; i++) {
            sum += powf( si_[i] - si_[i-1], 2.f );
        }

        // a) update beta by its expectation value
        //beta2_ = LW1_+1 / sum;

        // b) sample beta2 from the Gamma distribution
*/
/*
        std::default_random_engine generator;
        std::gamma_distribution<double> distribution( (LW1_+1)/2, sum/2);
        beta2_ = distribution(generator);
*//*


        std::cout << "N0=" << N0_ << ", N=" << posN_
                  << ", LW1=" << LW1_ << ", norm=" << norm_
                  << ", sum=" << sum << ", beta=" << (LW1_+1) / sum
                  << std::endl;

    } else if( method_flag == 3 ) {
        // prior penalising kinks in the positional preference profile
        for (size_t i = 0; i < LW1_; i++) {
            b_vector_[i] = (Ni_[i] - (posN_ - N0_) * pi_[i+1]) * 4 / beta2_;
        }

        Eigen::ConjugateGradient<Eigen::MatrixXf, Eigen::Lower | Eigen::Upper> cg;
        Eigen::MatrixXf B_matrix = getBmatrix(LW1_);
        cg.compute(B_matrix);
        cg.setMaxIterations(5);
        si_ = cg.solve(b_vector_);

        // normalize pi
        norm_ = 1.e-5f;
        for (size_t i = 1; i <= LW1_; i++) {
            pi_[i] = expf(si_[i-1]);
            norm_ += pi_[i];
        }
        for (size_t i = 1; i <= LW1_; i++) {
            pi_[i] /= norm_;
        }

        float sum = 1.e-5f;
        for (size_t i = 1; i < LW1_-1; i++) {
            sum += powf(si_[i] - (si_[i-1] + si_[i+1]) / 2, 2.f);
        }
        // a) update beta by its expectation value 2
        beta2_ = LW1_ / sum;

        // b) update beta by its expectation value 2
        //beta2_ = ( LW1-2 ) / sum;

        std::cout << "N0=" << N0_ << ", N=" << posN_
                  << ", LW1=" << LW1_ << ", norm=" << norm_
                  << ", sum=" << sum << ", beta=" << (LW1_+1) / sum
                  << std::endl;

    } else if( method_flag == 4 ){
        */
/**
         * Use L-BFGS as optimizer
         *//*


        // pre-define parameters
        LBFGSpp::LBFGSParam<float> param;
        param.epsilon = 1e-6f;
        param.max_iterations = 100;

        // create colver and function object
        LBFGSpp::LBFGSSolver<float> solver( param );

        // solve the function
        Eigen::VectorXf grad = Eigen::VectorXf::Zero( LW1_ );

        ObjFun func( LW1_, posN_, N0_, beta2_, Ni_, A_matrix_ );

        float fx;
        int niter;
        niter = solver.minimize( func, si_, fx );
        if( verbose_ ){
            std::cout << niter << " iterations, f(x)=" << fx << std::endl;
        }

        float beta2 = 0.f;
        for( size_t i = 1; i < LW1_; i++){
            beta2 += powf(si_[i] - si_[i-1], 2.f);
        }
        beta2 = ( LW1_-1 ) / beta2;
        //beta2_ = beta2;
        if( verbose_ ){
            std::cout << "Optimized beta2 = " << beta2 << std::endl;
        }

    }

    // update pos_ni by pi_i
    for( size_t i = 1; i <= LW1_; i++ ) {
        for (size_t n = 0; n < posN_; n++) {
            pos_[n][i] = pi_[i] * q_;
        }
    }

}

float EM::obj_fun( Eigen::VectorXf& si, Eigen::VectorXf& grad ){

    float Q;
    float p1 = 0.f;
    float p2 = 0.f;
    float sumexp = 0.f;

    // calculate objective function
    for( size_t i = 0; i < LW1_; i++ ){
        sumexp += expf( si[i] );
    }
    for( size_t i = 0; i < LW1_; i++ ){
        p1 += Ni_[i] * ( si[i] - logf( sumexp ) );
    }
    for( size_t i = 2; i < LW1_; i++ ){
        p2 += beta2_ * powf( si[i] - si[i-1], 2.f );
    }

    Q = p2 - p1;

    // update the gradients
    Eigen::VectorXf dot_product = A_matrix_ * si;
    for( size_t i = 0; i < LW1_; i++ ){
        grad[i] = - ( Ni_[i] - (posN_ - N0_) * expf( si[i] ) / sumexp -
                beta2_ * dot_product[i] );
    }

    return Q;
}

Eigen::MatrixXf EM::getAmatrix( size_t w ) {
    // define matrix A:
    Eigen::MatrixXf A_matrix = Eigen::MatrixXf::Zero( w, w );
    for( size_t i = 0; i < w; i++ ){
        for( size_t j = 0; j < w; j++ ){
            if( i == j ) {
                A_matrix(i, j) = 2;
            } else if( abs( i-j ) == 1 ){
                A_matrix(i, j) = -1;
            }
        }
    }
    A_matrix(0, 0) = 1;
    A_matrix(w-1, w-1) = 1;

    return A_matrix;
}

Eigen::MatrixXf EM::getBmatrix( size_t w ) {
    // define matrix B:
    Eigen::MatrixXf B_matrix = Eigen::MatrixXf::Zero( w, w );

    for( size_t i = 0; i < w; i++ ){
        for( size_t j = 0; j < w; j++ ){
            if( i == j ) {
                B_matrix(i, j) = 6;
            } else if( std::abs( i-j ) == 1 ){
                B_matrix(i, j) = -4;
            } else if( std::abs( i-j ) == 2 ){
                B_matrix(i, j) = 1;
            }
        }
    }
    B_matrix(0, 0) = B_matrix(w-1, w-1) = 1;
    B_matrix(1, 1) = B_matrix(w-2, w-2) = 5;
    B_matrix(0, 1) = B_matrix(1, 0) = B_matrix(w-1, w-2) = B_matrix(w-2, w-1) = -2;

    return B_matrix;
}

*/

float** EM::getR(){
    return r_;
}

float EM::getQ(){
    return q_;
}

void EM::print(){

    std::cout << std::fixed << std::setprecision( 2 );

    std::cout << " _________________________" << std::endl;
    std::cout << "|                         |" << std::endl;
    std::cout << "| k-mer Fractional Counts |" << std::endl;
    std::cout << "|_________________________|" << std::endl;
    std::cout << std::endl;

    for( size_t j = 0; j < W_; j++ ){
        for( size_t k = 0; k < K_+1; k++ ){
            float sum = 0.f;
            for( size_t y = 0; y < Y_[k+1]; y++ ) {
                std::cout << n_[k][y][j] << '\t';
                sum += n_[k][y][j];
            }
            std::cout << "sum=" << sum << std::endl;
        }
    }

    std::cout << std::fixed << std::setprecision( 4 );

    std::cout << " ____________________________" << std::endl;
    std::cout << "|                            |" << std::endl;
    std::cout << "| Conditional Probabilities  |" << std::endl;
    std::cout << "|____________________________|" << std::endl;
    std::cout << std::endl;

    for( size_t j = 0; j < W_; j++ ){
        for( size_t k = 0; k < K_+1; k++ ){
            float sum = 0.f;
            for( size_t y = 0; y < Y_[k+1]; y++ ) {
                std::cout << motif_->getV()[k][y][j] << '\t';
                sum += motif_->getV()[k][y][j];
            }
            std::cout << "sum=" << sum <<  std::endl;
        }
    }

    std::cout << " ______________________" << std::endl;
    std::cout << "|                      |" << std::endl;
    std::cout << "| Total Probabilities  |" << std::endl;
    std::cout << "|______________________|" << std::endl;
    std::cout << std::endl;

    for( size_t j = 0; j < W_; j++ ){
        for( size_t k = 0; k < K_+1; k++ ){
            float sum = 0.f;
            for( size_t y = 0; y < Y_[k+1]; y++ ) {
                std::cout << motif_->getP()[k][y][j] << '\t';
                sum += motif_->getP()[k][y][j];
            }
            std::cout << "sum=" << sum << std::endl;
        }
    }

    std::cout << " ________________" << std::endl;
    std::cout << "|                |" << std::endl;
    std::cout << "| Motifs Scores  |" << std::endl;
    std::cout << "|________________|" << std::endl;
    std::cout << std::endl;

    for( size_t j = 0; j < W_; j++ ){
        float sum = 0.f;
        for( size_t y = 0; y < Y_[K_+1]; y++ ) {
            std::cout << s_[y][j] << '\t';
            sum += s_[y][j];
        }
        std::cout << "sum=" << sum << std::endl;
    }

    std::cout << std::endl << "Fraction parameter q="
              << std::setprecision( 4 ) << q_ << std::endl;
}

void EM::printR(){

    // print out weights r for each position on each sequence

    std::cout << " ____________" << std::endl;
    std::cout << "|            |" << std::endl;
    std::cout << "| Posterior  |" << std::endl;
    std::cout << "|____________|" << std::endl;
    std::cout << std::endl;

    for( size_t n = 0; n < seqs_.size(); n++ ){
        std::cout << "seq " << n << ":" << std::endl;
        size_t L = seqs_[n]->getL();
        float sum = 0.f;
        std::cout << r_[n][0] << '\t';
        sum += r_[n][0];
        for( size_t i = 1; i <= L-W_+1; i++ ){
            std::cout << r_[n][L-i] << '\t';
            sum += r_[n][L-i];
//            std::cout << r_[n][i] << '\t';
//            sum += r_[n][i];
        }
/*
        // the unused positions
        for( size_t i = 1; i <= W_-2; i++ ){
            std::cout << r_[n][i] << '\t';
            sum += r_[n][i];
        }
        for( size_t i = L; i <= L+W_; i++ ){
            std::cout << r_[n][i] << '\t';
            sum += r_[n][i];
        }
*/
        std::cout << /*"sum=" << sum <<*/ std::endl;
        assert( fabsf( sum-1.0f ) < 1.e-4f );
    }
}

void EM::write( char* odir, std::string basename ){

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
        L = Global::ss ? L : ( L - 1 ) / 2;

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
			for( size_t i = 1; i <= seqs_[n]->getL()-W_+1; i++ ){
				ofile_r << std::setprecision( 2 ) << r_[n][seqs_[n]->getL()-i] << ' ';
			}
			ofile_r << std::endl;
		}
	}
}

