//
// Created by wanwan on 16.08.17.
//

#ifndef GIBBSSAMPLING_H_
#define GIBBSSAMPLING_H_

#include "../init/BackgroundModel.h"
#include "../init/MotifSet.h"
#include "EM.h"

class GibbsSampling {

public:

    GibbsSampling( Motif* motif, BackgroundModel* bg, std::vector<Sequence*> seqs, float q,
				   float beta = 7.0f, float gamma = 3.0f,
				   bool initializeZ = true, bool samplingZ = true, bool samplingQ = true,
				   bool optimizeA = true, bool GibbsMHalphas = false, bool dissampleAlphas = false );
	~GibbsSampling();

	void 					optimize();			// optimize BaMM model with Gibbs sampling

	void					print();			// print out optimized model v

	void					write( char* odir, std::string basename, bool ss );
												// write out the optimized (hyper-)parameters

private:

	Motif* 					motif_;				// motif to optimize within the EM
	BackgroundModel*		bg_;				// background model

	size_t 					K_;					// the order of the motif model
	size_t					W_;					// the width of the motif pattern
	float** 				A_;	        		// pseudo-count hyper-parameter for order k and motif position j
	size_t 					K_bg_;				// the order of the background model

	float** 				r_;		        	// responsibilities at all the positions in sequence n
                                                // Note: here the r_[n][0] indicates the responsibility of not having
                                                //      a motif on the sequence;
                                                //      r_[n][i] (for i > 0) indicates the responsibility of having a motif
                                                //      on position L-W+2-i

	float**					s_;					// log odds scores
	float*** 				n_;	            	// fractional counts n for (k+1)-mers y at motif position j
	size_t*					z_;					// observed position of motif in each sequence
	float**					pos_;				// positional prior, pos[i][0] indicates the prior for no motif present on sequence i

	float 					q_; 				// hyper-parameter q specifies the fraction of sequences containing motif
	std::vector<Sequence*>	seqs_;				// copy positive sequences
	size_t					N0_ = 0;			// count of sequences that do not contain a motif

	float 					llikelihood_ = 0.0f;// log likelihood for each iteration
	size_t					maxCGSIterations_ = 100;
	float					beta_;
	float					gamma_;

	float 					eta_ = 0.2f;		// learning rate for alpha learning
	double**				m1_t_;				// first moment for alpha optimizer (ADAM)
	double**				m2_t_;				// second moment for alpha optimizer (ADAM)
	std::mt19937			rngx_;

	std::vector<size_t>		Y_;

	bool					initializeZ_;
	bool					samplingZ_;
	bool					samplingQ_;
	bool					optimizeA_;
	bool					GibbsMHalphas_;
	bool					dissampleAlphas_;

							// sample motif position z by collapsed Gibbs sampling
	void					Collapsed_Gibbs_sampling_z();

							// sample sequence fraction q for motif by regular Gibbs sampling
	void					Gibbs_sample_q();

							// update alphas for all the orders up to K by stochastic gradient descent
	void					Optimize_alphas_by_SGD_ADAM( size_t order, size_t width, float learning_rate, size_t t );

							// Gibbs sampling alphas with Metropolis-Hastings algorithm
	void					GibbsMH_sample_alphas( size_t iteration );

							// sampling a's from the distribution of the log posterior
	void					Discrete_sample_alphas( size_t iteration );

							// calculate the gradient of the log posterior of alphas
	float					calc_gradient_alphas( float** alphas, size_t order, size_t position );

							// calculate the log posterior of a's
	float					calc_logCondProb_a( size_t iteration, float a, size_t order, size_t position );

							// calculate the prior of alphas
	float					calc_prior_alphas( float** alphas, size_t order );

							// calculate the log likelihood of alphas
	float					calc_llikelihood_alphas( float** alphas, size_t order );

							// calculate the log posterior of alphas
	float					calc_lposterior_alphas( float** alphas, size_t order );

};

#endif //GIBBSSAMPLING_H_
