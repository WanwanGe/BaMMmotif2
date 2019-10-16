#include <random>
#include <fstream>		// std::fstream
#include "Motif.h"

Motif::Motif( size_t length ){

	W_ = length;
	K_ = Global::modelOrder;
	C_ = 0;
    q_ = Global::q;

    k_bg_ = Global::bgModelOrder;

	v_ = ( float*** )calloc( K_+1, sizeof( float** ) );
	n_ = ( int*** )calloc( K_+1, sizeof( int** ) );
	p_ = ( float*** )calloc( K_+1, sizeof( float** ) );
	A_ = ( float** )calloc( K_+1, sizeof( float* ) );
	for( size_t k = 0; k < K_+1; k++ ){
		v_[k] = ( float** )calloc( Global::A2powerK[k+1], sizeof( float* ) );
		n_[k] = ( int** )calloc( Global::A2powerK[k+1], sizeof( int* ) );
		p_[k] = ( float** )calloc( Global::A2powerK[k+1], sizeof( float* ) );
		for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ){
			v_[k][y] = ( float* )calloc( W_, sizeof( float ) );
			n_[k][y] = ( int* )calloc( W_, sizeof( int ) );
			p_[k][y] = ( float* )calloc( W_, sizeof( float ) );
		}
		A_[k] = ( float* )calloc( W_, sizeof( float ) );
		for( size_t j = 0; j < W_; j++ ){
			A_[k][j] = Global::modelAlpha[k];
		}
	}

	s_ = ( float** )calloc( Global::A2powerK[K_+1], sizeof( float* ) );
	for( size_t y = 0; y < Global::A2powerK[K_+1]; y++ ){
		s_[y] = ( float* )calloc( W_, sizeof( float ) );
	}

    l_offset_ = 0;
    r_offset_ = 0;

    srand( 42 );
}

Motif::Motif( const Motif& other ){ 		// copy constructor

	W_ = other.W_;
	K_ = other.K_;
	A_ = other.A_;
	C_ = other.C_;
    q_ = other.q_;

	v_ = ( float*** )malloc( ( K_+1 ) * sizeof( float** ) );
	n_ = ( int*** )malloc( ( K_+1 ) * sizeof( int** ) );
	p_ = ( float*** )malloc( ( K_+1 ) * sizeof( float** ) );
	A_ = ( float** )malloc( ( K_+1 ) * sizeof( float* ) );
	for( size_t k = 0; k < K_+1; k++ ){
		v_[k] = ( float** )malloc( Global::A2powerK[k+1] * sizeof( float* ) );
		n_[k] = ( int** )malloc( Global::A2powerK[k+1] * sizeof( int* ) );
		p_[k] = ( float** )malloc( Global::A2powerK[k+1] * sizeof( float* ) );
		for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ){
			v_[k][y] = ( float* )malloc( W_ * sizeof( float ) );
			n_[k][y] = ( int* )malloc( W_ * sizeof( int ) );
			p_[k][y] = ( float* )malloc( W_ * sizeof( float ) );
			for( size_t j = 0; j < W_; j++ ){
				v_[k][y][j] = other.v_[k][y][j];
				p_[k][y][j] = other.p_[k][y][j];
				n_[k][y][j] = other.n_[k][y][j];
			}
		}
		A_[k] = ( float* )malloc( W_ * sizeof( float ) );
		for( size_t j = 0; j < W_; j++ ){
			A_[k][j] = other.A_[k][j];
		}
	}

	s_ = ( float** )malloc( Global::A2powerK[K_+1] * sizeof( float* ) );
	for( size_t y = 0; y < Global::A2powerK[K_+1]; y++ ){
		s_[y] = ( float* )malloc( W_ * sizeof( float ) );
		for( size_t j = 0; j < W_; j++ ){
			s_[y][j] = other.s_[y][j];
		}
	}

    l_offset_ = other.l_offset_;
    r_offset_ = other.r_offset_;

	k_bg_ = other.k_bg_;

	isInitialized_ = true;
}

Motif::~Motif(){

	for( size_t k = 0; k < K_+1; k++ ){
		for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ){
			free( v_[k][y] );
			free( n_[k][y] );
			free( p_[k][y] );
		}
		free( v_[k] );
		free( n_[k] );
		free( p_[k] );
		free( A_[k]);
	}
	free( v_ );
	free( A_ );
	free( n_ );
	free( p_ );

	for( size_t y = 0; y < Global::A2powerK[K_+1]; y++ ){
		free( s_[y] );
	}
	free( s_ );

}

// initialize v from binding sites file
void Motif::initFromBindingSites( char* indir, size_t l_flank, size_t r_flank ){

	std::ifstream file( indir );			// read file
	std::string bindingsite;				// get binding site sequence from each line
	size_t bindingSiteWidth;				// length of binding site

	while( getline( file, bindingsite ).good() ){

		C_++;								// count the number of binding sites

		// add alphabets randomly at the beginning of each binding site
		for( size_t i = 0; i < l_flank; i++ )
			bindingsite.insert( bindingsite.begin(),
                                Alphabet::getBase( static_cast<uint8_t>( rand() )
                                                   % static_cast<uint8_t>( Global::A2powerK[1] ) + 1 ));

		// add alphabets randomly at the end of each binding site
		for( size_t i = 0; i < r_flank; i++ )
			bindingsite.insert( bindingsite.end(),
                                Alphabet::getBase( static_cast<uint8_t>( rand() )
                                                   % static_cast<uint8_t>( Global::A2powerK[1] ) + 1 ));
		bindingSiteWidth = bindingsite.length();

		if( bindingSiteWidth != W_ ){	// all binding sites have the same length
			fprintf( stderr, "Error: Length of binding site on line %d differs.\n"
					"Binding sites should have the same length.\n", ( int )C_ );
			exit( 1 );
		}
		if( bindingSiteWidth < K_+1 ){	// binding sites should be longer than the order of model
			fprintf( stderr, "Error: Length of binding site sequence "
					"is shorter than model order.\n" );
			exit( 1 );
		}

		// scan the binding sites and calculate k-mer counts n
		for( size_t k = 0; k < K_+1; k++ ){
			for( size_t j = 0; j < bindingSiteWidth; j++ ){
				size_t y = 0;
				for( size_t a = 0; a < k+1; a++ ){
					// calculate y based on (k+1)-mer bases
                    if( j >= a ) {
                        y += Global::A2powerK[a] * (Alphabet::getCode(bindingsite[j-a]) - 1);
                    } else {
                        y += Global::A2powerK[a] * (static_cast<uint8_t>( rand() )
                                      % static_cast<uint8_t>( Global::A2powerK[1] ));
                    }
				}
				n_[k][y][j]++;
			}
		}
	}

	// calculate v and p from k-mer counts n
	calculateV( n_ );
	calculateP();

	// set isInitialized
	isInitialized_ = true;

}

// initialize v from PWM file
void Motif::initFromPWM( float** PWM, SequenceSet* posSeqset, float q ){

    q_ = q;

	// set k-mer counts to zero
	for( size_t k = 0; k < K_+1; k++ ){
		for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ){
			for( size_t j = 0; j < W_; j++ ){
				n_[k][y][j] = 0;
			}
		}
	}

	// for k = 0, obtain v from PWM:
	for( size_t j = 0; j < W_; j++ ){
		float norm = 0.0f;
		for( size_t y = 0; y < Global::A2powerK[1]; y++ ){
			if( PWM[y][j] <= Global::eps ){
                v_[0][y][j] = Global::eps;
            } else {
                v_[0][y][j] = PWM[y][j];
            }
			norm += v_[0][y][j];
		}
		// normalize PWMs to sum the weights up to 1
		for( size_t y = 0; y < Global::A2powerK[1]; y++ ){
			v_[0][y][j] /= norm;
		}
	}

	// learn higher-order model based on the weights from the PWM
	// compute log odd scores s[y][j], log likelihoods of the highest order K
	std::vector<std::vector<float>> score;
	score.resize( Global::A2powerK[1] );
	for( size_t y = 0; y < Global::A2powerK[1]; y++ ){
		score[y].resize( W_ );
	}
	for( size_t y = 0; y < Global::A2powerK[1]; y++ ){
		for( size_t j = 0; j < W_; j++ ){
			score[y][j] = v_[0][y][j] * Global::A2powerK[1];
		}
	}

	// sampling z from each sequence of the sequence set based on the weights:
	std::vector<Sequence*> posSet = posSeqset->getSequences();

    size_t count = 0;
    std::vector<Sequence*>::iterator it = posSet.begin();
    while( it != posSet.end() ){
        if( (*it)->getL() < W_ ){
            //std::cout << "Warning: remove the short sequence: "
            // << (*it)->getHeader() << std::endl;
            posSet.erase(it);
            count++;
        } else {
            it++;
        }
    }

    if( count > 0 ){
        std::cout << "Note: " << count << " short sequences have been "
                  << "neglected for sampling PWM." << std::endl;
    }

//#pragma omp for
	for( size_t n = 0; n < posSet.size(); n++ ){

		size_t LW1 = posSet[n]->getL() - W_ + 1;

		// motif position
		size_t z;

		// get the kmer array
		size_t* kmer = posSet[n]->getKmer();

		// calculate responsibilities over all LW1 positions on n'th sequence
		std::vector<float> r;
		r.resize( LW1 + 1 );

		std::vector<float> posteriors;
		float normFactor = 0.0f;
		// calculate positional prior:
		float pos0 = 1.0f - q;
		float pos1 = q / static_cast<float>( LW1 );

		// todo: could be parallelized by extracting 8 sequences at once
		// todo: should be written in a faster way
		for( size_t i = 1; i <= LW1; i++ ){
			r[i] = 1.0f;
			for( size_t j = 0; j < W_; j++ ){
				// extract monomers from motif at position i
				// over W of the n'th sequence
				size_t y = kmer[i-1+j] % Global::A2powerK[1];
				r[i] *= score[y][j];
			}
			r[i] *= pos1;
			normFactor += r[i];
		}
		// for sequences that do not contain motif
		r[0] = pos0;
		normFactor += r[0];
		for( size_t i = 0; i <= LW1; i++ ){
			r[i] /= normFactor;
			posteriors.push_back( r[i] );
		}

        // todo: with random drawing from std::discrete_distribution,
        // todo: it is difficult to parallelize the code
        // todo: possible solution is to implement this function on our
        // todo: own and pre-generated n random numbers for drawing z
		// draw a new position z from discrete posterior distribution
		std::discrete_distribution<size_t> posterior_dist( posteriors.begin(),
                                                           posteriors.end() );

		// draw a sample z randomly
		z = posterior_dist( Global::rngx );

		// count kmers with sampled z
		if( z > 0 ){
			for( size_t k = 0; k < K_+1; k++ ){
				for( size_t j = 0; j < W_; j++ ){
					size_t y = kmer[z-1+j] % Global::A2powerK[k+1];
                    //__sync_fetch_and_add(&(n_[k][y][j]), 1);
                    n_[k][y][j]++;
				}
			}
		}
	}

	// calculate motif model from counts for higher order
	// for k > 0:
	for( size_t k = 1; k < K_+1; k++ ){
		for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ){
			size_t y2 = y % Global::A2powerK[k];    // cut off first nt in (k+1)-mer y
			size_t yk = y / Global::A2powerK[1];    // cut off last nt in (k+1)-mer y
			for( size_t j = 0; j < k; j++ ){// when j < k, i.e. p(A|CG) = p(A|C)
				v_[k][y][j] = v_[k-1][y2][j];
			}
			for( size_t j = k; j < W_; j++ ){
				v_[k][y][j] = ( n_[k][y][j] + A_[k][j] * v_[k-1][y2][j] )
							/ ( n_[k-1][yk][j-1] + A_[k][j] );
			}
		}
	}
	// calculate probabilities p
	calculateP();

	// set isInitialized
	isInitialized_ = true;

}

// initialize v from Bayesian Markov model file and set isInitialized
void Motif::initFromBaMM( char* indir, size_t l_flank, size_t r_flank ){

	std::ifstream file( indir, std::ifstream::in );
	std::string line;
    if( file.is_open() ) {

        // loop over motif position j
        // set each v to 0.25f in the flanking region
        for (size_t j = 0; j < l_flank; j++) {
            for (size_t k = 0; k < K_ + 1; k++) {
                for (size_t y = 0; y < Global::A2powerK[k+1]; y++) {
                    v_[k][y][j] = 1.0f / static_cast<float>( Global::A2powerK[1] );
                }
            }
        }

        // read in the v's from the bamm file for the core region
        for (size_t j = l_flank; j < W_ - r_flank; j++) {

            // loop over order k
            for (size_t k = 0; k < K_ + 1; k++) {

                getline(file, line);

                std::stringstream number(line);

                for (size_t y = 0; y < Global::A2powerK[k + 1]; y++) {
                    number >> v_[k][y][j];
                }
            }
            // read 'empty' line
            getline(file, line);
        }

        // set each v to 0.25f in the flanking region
        for (size_t j = W_ - r_flank; j < W_; j++) {
            for (size_t k = 0; k < K_ + 1; k++) {
                for (size_t y = 0; y < Global::A2powerK[k + 1]; y++) {
                    v_[k][y][j] = 1.0f / static_cast<float>( Global::A2powerK[1] );
                }
            }
        }

        // calculate probabilities p
        calculateP();

        // set isInitialized
        isInitialized_ = true;

    } else {
        std::cerr << "Error: Input BaMM file cannot be opened!" << std::endl;
        exit( 1 );
    }
}

float** Motif::getS(){
	return s_;
}

size_t Motif::getLOffset() {
    return l_offset_;
}

size_t Motif::getROffset() {
    return r_offset_;
}

void Motif::optimizeMotifLength(){

    /**
     *  optimize motif length according to optimized alphas
     */
    float alpha_zero_cutoff = 4.f;
    size_t min_motif_length = 4;
    //float alpha_one_cutoff = 16.f;

    size_t l_pointer = 0;
    size_t r_pointer = W_;
    for (size_t j = 0; j < W_; j++ ){
        if ( A_[1][j] > alpha_zero_cutoff ){
            l_pointer ++;
        } else {
            break;
        }
    }
    for (size_t j = 0; j < W_; j++ ){
        if ( A_[1][W_-j] > alpha_zero_cutoff ){
            r_pointer ++;
        } else {
            break;
        }
    }
    if (r_pointer - l_pointer > min_motif_length ){
        l_offset_ = l_pointer;
        r_offset_ = r_pointer;
    } else {
        std::cout << "Warning: the cutoff of alpha for motif length optimization is not proper.\n"
                  << "Please either make it larger or first extend core motif length.\n";
    }
}

void Motif::copyOptimalMotif(Motif *motif, size_t l_offset, size_t r_offset){

    // get the v's from the original motif with only the core regions
    for (size_t j = 0; j < motif->getW() - l_offset - r_offset; j++) {
        // loop over order k
        for (size_t k = 0; k < K_ + 1; k++) {
            for (size_t y = 0; y < Global::A2powerK[k + 1]; y++) {
                v_[k][y][j] = motif->getV()[k][y][j+l_offset];
            }
            A_[k][j] = motif->getA()[k][j+l_offset];
        }
    }

    // calculate probabilities p
    calculateP();

}

void Motif::calculateV( int*** n ){
	// Note: This function is written for reading in binding site files.

    // for k = 0, v_ = freqs:
	for( size_t y = 0; y < Global::A2powerK[1]; y++ ){
		for( size_t j = 0; j < W_; j++ ){
			v_[0][y][j] = ( n[0][y][j] + A_[0][j] / Global::A2powerK[1] )
						/ ( static_cast<float>( C_ ) + A_[0][j] );
        }
	}

	// for k > 0:
	for( size_t k = 1; k < K_+1; k++ ){
		for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ){
			size_t y2 = y % Global::A2powerK[k];    // cut off first nucleotide in (k+1)-mer y
			size_t yk = y / Global::A2powerK[1];    // cut off last nucleotide in (k+1)-mer y
			for( size_t j = 0; j < k; j++ ){        // when j < k, i.e. p_j(A|CG) = p_j(A|C)
				v_[k][y][j] = v_[k-1][y2][j];
			}
			for( size_t j = k; j < W_; j++ ){
				v_[k][y][j] = ( n[k][y][j] + A_[k][j] * v_[k-1][y2][j] )
							/ ( n[k-1][yk][j-1] + A_[k][j] );
			}
		}
	}
}

void Motif::calculateP(){
	// calculate probabilities, i.e. p_j(ACG) = p_j(G|AC) * p_j-1(AC)

	// when k = 0:
	for( size_t j = 0; j < W_; j++ ){
		for( size_t y = 0; y < Global::A2powerK[1]; y++ ){
			p_[0][y][j] = v_[0][y][j];
		}
	}

	// when k > 0:
	for( size_t k = 1; k < K_+1; k++ ){
		for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ){
			size_t yk = y / Global::A2powerK[1];    // cut off last nucleotide in (k+1)-mer
			// when j < k:
			for( size_t j = 0; j < k; j++ ){
                p_[k][y][j] = v_[k][y][j] / Global::A2powerK[k];
			}
            // when j >= k:
			for( size_t j = k; j < W_; j++ ){
				p_[k][y][j] = v_[k][y][j] * p_[k-1][yk][j-1];
			}
		}
	}
}

void Motif::calculateLogS( float** Vbg ){

    // todo: this could be wrong
    //size_t k_bg = ( K_ > k_bg_ ) ? k_bg_ : K_;
    size_t  k_bg = k_bg_;
	for( size_t y = 0; y < Global::A2powerK[K_+1]; y++ ){
		size_t y_bg = y % Global::A2powerK[k_bg+1];
		for( size_t j = 0; j < W_; j++ ){
			s_[y][j] = logf( v_[K_][y][j] + Global::eps ) - logf( Vbg[k_bg][y_bg] );
		}
	}

}

void Motif::print(){

    std::cout << std::fixed << std::setprecision( 4 );

    std::cout << " ______________" << std::endl
              << "|              |" << std::endl
              << "| k-mer Counts |" << std::endl
              << "|______________|" << std::endl
              << std::endl;

    for( size_t j = 0; j < W_; j++ ){
        for( size_t k = 0; k < K_+1; k++ ){
            int sum = 0;
            for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ) {
                std::cout << n_[k][y][j] << '\t';
                sum += n_[k][y][j];
            }
            std::cout << "sum=" << sum << std::endl;
        }
    }

    std::cout << " ___________________________" << std::endl
              << "|                           |" << std::endl
              << "| Conditional Probabilities |" << std::endl
              << "|___________________________|" << std::endl
              << std::endl;

	for( size_t j = 0; j < W_; j++ ){
		for( size_t k = 0; k < K_+1; k++ ){
            float sum = 0.f;
			for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ) {
                std::cout << v_[k][y][j] << '\t';
                sum += v_[k][y][j];
            }
			std::cout << "sum=" << sum << std::endl;
//            assert( fabsf( sum - Global::A2powerK[k] ) < 1.e-3f );
		}
	}

    std::cout << " ____________________" << std::endl
              << "|                    |" << std::endl
              << "| Full Probabilities |" << std::endl
              << "|____________________|" << std::endl
              << std::endl;

    for( size_t j = 0; j < W_; j++ ){
        for( size_t k = 0; k < K_+1; k++ ){
            float sum = 0.f;
            for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ) {
                std::cout << p_[k][y][j] << '\t';
                sum += p_[k][y][j];
            }
            std::cout << "sum=" << sum << std::endl;
//            assert( fabsf( sum-1.0f ) < 1.e-4f );
        }
    }

}

void Motif::printS(){
    std::cout << " _________________" << std::endl
              << "|                 |" << std::endl
              << "| Log Odds Scores |" << std::endl
              << "|_________________|" << std::endl
              << std::endl;

    std::vector<float> sum_score;
    sum_score.resize(W_);
    for( size_t j = 0; j < W_; j++ ) {
        sum_score[j] = 0.f;
    }
    for (size_t y = 0; y < Global::A2powerK[K_+1]; y++) {
        for( size_t j = 0; j < W_; j++ ) {
            std::cout << std::setprecision(2) << s_[y][j] << '\t';
            sum_score[j] += s_[y][j];
        }
        std::cout << std::endl;
    }

    // print the sum of scores on each position
    std::cout << "sum scores at each position:" << std::endl;
    for( size_t j = 0; j < W_; j++ ) {
        std::cout << sum_score[j] << '\t';
    }
    std::cout << std::endl;
}

void Motif::write( char* odir, std::string basename ){

	/**
	 * save motif learned by BaMM in two flat files:
	 * (1) posSequenceBasename.ihbcp: 		conditional probabilities after EM
	 * (2) posSequenceBasename.ihbp: 		probabilities of PWM after EM
	 */

	std::string opath = std::string( odir )  + '/' + basename;

	// output conditional probabilities v[k][y][j]
    // and probabilities p[k][y][j]
	std::string opath_v = opath + ".ihbcp"; 	// ihbcp: inhomogeneous bamm
												// conditional probabilities
	std::string opath_p = opath + ".ihbp";		// ihbp: inhomogeneous bamm
												// probabilities
	std::ofstream ofile_v( opath_v.c_str() );
	std::ofstream ofile_p( opath_p.c_str() );

	for( size_t j = 0; j < W_; j++ ){
		for( size_t k = 0; k < K_+1; k++ ){
			for( size_t y = 0; y < Global::A2powerK[k+1]; y++ ){
				ofile_v << std::scientific << std::setprecision(3)
						<< v_[k][y][j] << ' ';
				ofile_p << std::scientific << std::setprecision(3)
						<< p_[k][y][j] << ' ';
			}
			ofile_v << std::endl;
			ofile_p << std::endl;
		}
		ofile_v << std::endl;
		ofile_p << std::endl;
	}
}
