#include "BackgroundModel.h"

BackgroundModel::BackgroundModel( SequenceSet& sequenceSet,
									size_t order,
									std::vector<float> alpha,
									bool interpolate,
									std::vector<std::vector<size_t>> foldIndices,
									std::vector<size_t> folds ){

	//name_.assign( baseName( sequenceSet.getSequenceFilepath().c_str() ) );
	name_ = baseName( sequenceSet.getSequenceFilepath().c_str() );
	K_ = order;
	A_ = alpha;
	interpolate_ = interpolate;

	for( size_t k = 0; k < K_+8; k++ ){
		Y_.push_back( ipow( Alphabet::getSize(), k ) );
	}

	if( folds.empty() ){
		if( foldIndices.empty() ){

			folds.push_back( 0 );

			foldIndices.push_back( std::vector<size_t>() );
			for( size_t n = 0; n < sequenceSet.getN(); n++ ){
				foldIndices[0].push_back( n );
			}
		} else {
			folds.resize( foldIndices.size() );
			std::iota( std::begin( folds ), std::end( folds ), 0 );
		}
	} else if( foldIndices.empty() ){

		folds.clear();
		folds.push_back( 0 );

		foldIndices.push_back( std::vector<size_t>() );
		for( size_t n = 0; n < sequenceSet.getN(); n++ ){
			foldIndices[0].push_back( n );
		}
	}

	n_ = ( size_t** )malloc( ( K_+1 ) * sizeof( size_t* ) );
	for( size_t k = 0; k <= K_; k++ ){
		n_[k] = ( size_t* )calloc( Y_[k+1], sizeof( size_t ) );
	}

	// calculate counts
	std::vector<Sequence*> seqs = sequenceSet.getSequences();
	// loop over folds
	for( size_t f = 0; f < folds.size(); f++ ){
		// loop over fold indices
		for( size_t f_idx = 0; f_idx < foldIndices[folds[f]].size(); f_idx++ ){
			// get sequence index
			size_t s_idx = foldIndices[folds[f]][f_idx];
			// get sequence length
			size_t L = seqs[s_idx]->getL();

			int* kmer = seqs[s_idx]->getKmer();
			// loop over order
			// loop over sequence positions
			for( size_t i = 0; i < L; i++ ){
				if( kmer[i] >= 0 ){
					for( size_t k = 0; k <= K_; k++ ){
						// extract (k+1)mer
						size_t y = static_cast<size_t>( kmer[i] ) % Y_[k+1];
						// count (k+1)mer
						n_[k][y]++;
					}
				}
			}
		}
	}

	v_ = ( float** )malloc( ( K_+1 ) * sizeof( float* ) );
	for( size_t k = 0; k <= K_; k++ ){
		v_[k] = ( float* )calloc( Y_[k+1], sizeof( float ) );
	}
	// calculate conditional probabilities from counts
	calculateV();
}

BackgroundModel::BackgroundModel( std::string filePath ){

	name_.assign( baseName( filePath.c_str() ) );

	n_ = NULL;
	v_ = NULL;

	struct stat sb;

	if( stat( filePath.c_str(), &sb ) == 0 && S_ISREG( sb.st_mode ) ){

		FILE* file;
		if( ( file = fopen( filePath.c_str(), "r" ) ) != NULL ){

			int K;
			if( fscanf( file, "# K = %d\n", &K ) == 1 ){
				K_ = static_cast<size_t>( K );
			} else{
				std::cerr << "Error: Wrong BaMM format: " << filePath << std::endl;
				exit( -1 );
			}
			A_.resize( K_+1 );

			float A;
			if( fscanf( file, "# A = %e", &A ) == 1 ){
				A_[0] = A;
			} else{
				std::cerr << "Error: Wrong BaMM format: " << filePath << std::endl;
				exit( -1 );
			}
			for( size_t k = 1; k <= K_; k++ ){
				if( fscanf( file, "%e", &A ) == 1 ){
					A_[k] = A;
				} else {
					std::cerr << "Error: Wrong BaMM format: " << filePath << std::endl;
					exit( -1 );
				}
			}

			for( size_t k = 0; k < K_+8; k++ ){
				Y_.push_back( ipow( Alphabet::getSize(), k ) );
			}

			v_ = ( float** )malloc( ( K_+1 ) * sizeof( float* ) );
			for( size_t k = 0; k <= K_; k++ ){
				v_[k] = ( float* )calloc( Y_[k+1], sizeof( float ) );
			}

			float value;
			for( size_t k = 0; k <= K_; k++ ){
				for( size_t y = 0; y < Y_[k+1]; y++ ){

					if( fscanf( file, "%e", &value ) != EOF ){
						v_[k][y] = value;

					} else {

						std::cerr << "Error: Wrong BaMM format: " << filePath << std::endl;
						exit( -1 );
					}
				}
			}
			fclose( file );

		} else {

			std::cerr << "Error: Cannot open BaMM file: " << filePath << std::endl;
			exit( -1 );
		}
	}
}

BackgroundModel::~BackgroundModel(){

	if( n_ != NULL ){
		for( size_t k = 0; k <= K_; k++ ){
			free( n_[k] );
		}
		free( n_ );
	}
	if( v_ != NULL ){
		for( size_t k = 0; k <= K_; k++ ){
			free( v_[k] );
		}
		free( v_ );
	}
}

std::string BackgroundModel::getName(){
    return name_;
}

size_t BackgroundModel::getOrder(){
    return K_;
}

void BackgroundModel::expV(){

	for( size_t k = 0; k <= K_; k++ ){
		for( size_t y = 0; y < Y_[k+1]; y++ ){
			v_[k][y] = expf( v_[k][y] );
		}
	}
	vIsLog_ = false;
}

void BackgroundModel::logV(){

	for( size_t k = 0; k <= K_; k++ ){
		for( size_t y = 0; y < Y_[k+1]; y++ ){
			v_[k][y] = logf( v_[k][y] );
		}
	}
	vIsLog_ = true;
}

bool BackgroundModel::vIsLog(){
	return vIsLog_;
}

double BackgroundModel::calculateLogLikelihood( SequenceSet& sequenceSet,
		                                        std::vector<std::vector<size_t>> foldIndices,
		                                        std::vector<size_t> folds ){

	if( !( vIsLog_ ) ){
		logV();
	}

	if( folds.empty() ){
		if( foldIndices.empty() ){

			folds.push_back( 0 );

			foldIndices.push_back( std::vector<size_t>() );
			for( size_t n = 0; n < sequenceSet.getN(); n++ ){
				foldIndices[0].push_back( n );
			}
		} else {
			folds.resize( foldIndices.size() );
			std::iota( std::begin( folds ), std::end( folds ), 0 );
		}
	} else if( foldIndices.empty() ){

		folds.clear();
		folds.push_back( 0 );

		foldIndices.push_back( std::vector<size_t>() );
		for( size_t n = 0; n < sequenceSet.getN(); n++ ){
			foldIndices[0].push_back( n );
		}
	}

	double lLikelihood = 0.0;

	// loop over folds
	for( size_t f = 0; f < folds.size(); f++ ){
		// loop over fold indices
		for( size_t f_idx = 0; f_idx < foldIndices[folds[f]].size(); f_idx++ ){
			// get sequence index
			size_t s_idx = foldIndices[folds[f]][f_idx];
			// get sequence length
			size_t L = sequenceSet.getSequences()[s_idx]->getL();
			int* kmer = sequenceSet.getSequences()[s_idx]->getKmer();

			// loop over sequence positions
			for( size_t i = 0; i < L; i++ ){
				if( kmer[i] >= 0 ){
					// calculate k
					size_t k = std::min( i, K_ );
					// extract (k+1)mer
					size_t y = static_cast<size_t>( kmer[i] ) % Y_[k+1];
					// add log probabilities
					lLikelihood += v_[k][y];
				}
			}
		}
	}

	return lLikelihood;
}

void BackgroundModel::calculatePosLikelihoods( SequenceSet& sequenceSet,
		                                       char* outputDirectory,
		                                       std::vector<std::vector<size_t>> foldIndices,
		                                       std::vector<size_t> folds ){

	if( vIsLog_ ){
		expV();
	}

	if( folds.empty() ){
		if( foldIndices.empty() ){

			folds.push_back( 0 );

			foldIndices.push_back( std::vector<size_t>() );
			for( size_t n = 0; n < sequenceSet.getN(); n++ ){
				foldIndices[0].push_back( n );
			}
		} else{
			folds.resize( foldIndices.size() );
			std::iota( std::begin( folds ), std::end( folds ), 0 );
		}
	} else if( foldIndices.empty() ){

		folds.clear();
		folds.push_back( 0 );

		foldIndices.push_back( std::vector<size_t>() );
		for( size_t n = 0; n < sequenceSet.getN(); n++ ){
			foldIndices[0].push_back( n );
		}
	}

	std::ofstream file( std::string( outputDirectory ) + '/'
			            + name_ + '_'
			            + baseName( sequenceSet.getSequenceFilepath().c_str() )
			            + ".lhs" );

	if( file.is_open() ){

		// loop over folds
		for( size_t f = 0; f < folds.size(); f++ ){
			// loop over fold indices
			for( size_t f_idx = 0; f_idx < foldIndices[folds[f]].size(); f_idx++ ){
				// get sequence index
				size_t s_idx = foldIndices[folds[f]][f_idx];
				// get sequence length
				size_t L = sequenceSet.getSequences()[s_idx]->getL();
				int* kmer = sequenceSet.getSequences()[s_idx]->getKmer();
				// loop over sequence positions
				for( size_t i = 0; i < L; i++ ){
					if( kmer[i] >= 0 ){
						// calculate k
						size_t k = std::min( i, K_ );
						// extract (k+1)mer
						size_t y = static_cast<size_t>( kmer[i] ) % Y_[k+1];
						file << ( i == 0 ? "" : " " );
						file << std::scientific << std::setprecision( 6 ) << v_[k][y];
					}
				}
				file << std::endl;
			}
		}
		file.close();

	} else {

		std::cerr << "Error: Cannot write into output directory: " << outputDirectory << std::endl;
		exit( -1 );
	}
}

void BackgroundModel::print(){

	if( interpolate_ ){
		std::cout << " ___________________________________" << std::endl;
		std::cout << "|*                                 *|" << std::endl;
		std::cout << "| Homogeneous Bayesian Markov Model |" << std::endl;
		std::cout << "|*_________________________________*|" << std::endl;
		std::cout << std::endl;
	} else {
		std::cout << " __________________________" << std::endl;
		std::cout << "|*                        *|" << std::endl;
		std::cout << "| Homogeneous Markov Model |" << std::endl;
		std::cout << "|*________________________*|" << std::endl;
		std::cout << std::endl;
	}

	std::cout << "name = " << name_ << std::endl << std::endl;
	std::cout << "K = " << K_ << std::endl;
	std::cout << "A =";
	for( size_t k = 0; k <= K_; k++ ){
		std::cout << " " << A_[k];
	}
	std::cout << std::endl;

	if( n_ != NULL ){

		std::cout << " ________" << std::endl;
		std::cout << "|        |" << std::endl;
		std::cout << "| Counts |" << std::endl;
		std::cout << "|________|" << std::endl;
		std::cout << std::endl;

		for( size_t k = 0; k <= K_; k++ ){
			for( size_t y = 0; y < Y_[k+1]; y++ ){
				std::cout << n_[k][y] << " ";
			}
			std::cout << std::endl;
		}
	}

	std::cout << " ___________________________" << std::endl;
	std::cout << "|                           |" << std::endl;
	std::cout << "| Conditional probabilities |" << std::endl;
	std::cout << "|___________________________|" << std::endl;
	std::cout << std::endl;

	for( size_t k = 0; k <= K_; k++ ){
		std::cout << std::fixed << std::setprecision( 3 ) << v_[k][0];
		for( size_t y = 1; y < Y_[k+1]; y++ ){
			std::cout << " " << std::fixed << std::setprecision( 3 ) << v_[k][y];
		}
		std::cout << std::endl;
	}
}

void BackgroundModel::write( char* dir ){

    if( vIsLog_ ){
        expV();
    }

	std::ofstream file( std::string( dir ) + '/' + name_ + ( interpolate_ ? ".hbcp" : ".hnbcp" ) );
	if( file.is_open() ){

		file << "# K = " << K_ << std::endl;
		file << "# A =";
		for( size_t k = 0; k <= K_; k++ ){
			file << " " << A_[k];
		}
		file << std::endl;

		for( size_t k = 0; k <= K_; k++ ){
			for( size_t y = 0; y < Y_[k+1]; y++ ){
				file << std::scientific << std::setprecision( 6 ) << v_[k][y] << " ";
			}
			file << std::endl;
		}
		file.close();

	} else{

		std::cerr << "Error: Cannot write into output directory: " << dir << std::endl;
		exit( -1 );
	}

	// calculate probabilities from conditional probabilities
	float** p = ( float** )malloc( ( K_+1 ) * sizeof( float* ) );
	for( size_t k = 0; k <= K_; k++ ){
		p[k] = ( float* )calloc( Y_[k+1], sizeof( float ) );
	}

	// calculate probabilities for order k = 0
	for( size_t y = 0; y < Y_[1]; y++ ){
		p[0][y] = v_[0][y];
	}

	// calculate probabilities for order k > 0
	// e.g. p(ACGT) = p(T|ACG) * p(ACG)
	for( size_t k = 1; k <= K_; k++ ){
		for( size_t y = 0; y < Y_[k+1]; y++ ){
			// omit last base (e.g. ACG) from y (e.g. ACGT)
			size_t yk = y / Y_[1];
			p[k][y] = v_[k][y] * p[k-1][yk];
		}
	}

	file.clear();
	file.open( std::string( dir ) + '/' + name_ + ( interpolate_ ? ".hbp" : ".hnbp" ) );
	if( file.is_open() ){

		file << "# K = " << K_ << std::endl;
		file << "# A =";
		for( size_t k = 0; k <= K_; k++ ){
			file << " " << A_[k];
		}
		file << std::endl;

		for( size_t k = 0; k <= K_; k++ ){
			for( size_t y = 0; y < Y_[k+1]; y++ ){
				file << std::scientific << std::setprecision( 6 ) << p[k][y] << " " ;
			}
			file << std::endl;
		}
		file.close();

	} else {

		std::cerr << "Error: Cannot write into output directory: " << dir << std::endl;
		exit( -1 );
	}

	for( size_t k = 0; k <= K_; k++ ){
		free( p[k] );
	}
	free( p );
}

void BackgroundModel::calculateV(){

	size_t baseCounts = 0;
	for( size_t y = 0; y < Y_[1]; y++ ){
		baseCounts += n_[0][y];
	}

	// calculate probabilities for order k = 0
	for( size_t y = 0; y < Y_[1]; y++ ){
		v_[0][y] = ( static_cast<float>( n_[0][y] ) + A_[0] * 0.25f ) // cope with absent bases using pseudocounts
				   / ( static_cast<float>( baseCounts ) + A_[0] );
	}

	// calculate probabilities for order k > 0
	for( size_t k = 1; k <= K_; k++ ){

		for( size_t y = 0; y < Y_[k+1]; y++ ){
			// omit first base (e.g. CGT) from y (e.g. ACGT)
			size_t y2 = y % Y_[k];
			// omit last base (e.g. ACG) from y (e.g. ACGT)
			size_t yk = y / Y_[1];
			if( interpolate_ ){
				v_[k][y] = ( static_cast<float>( n_[k][y] ) + A_[k] * v_[k-1][y2] )
						   / ( static_cast<float>( n_[k-1][yk] ) + A_[k] );
			} else {
				v_[k][y] = ( static_cast<float>( n_[k][y] ) + A_[k] * 0.25f )
						   / ( static_cast<float>( n_[k-1][yk] ) + A_[k] );
			}
		}

	}
}
