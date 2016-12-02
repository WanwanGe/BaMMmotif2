#include <fstream>		// std::fstream

#include "MotifSet.h"

MotifSet::MotifSet(){

	if( Global::BaMMpatternFilename != NULL ){

		// scan file and conduct for IUPAC pattern
		std::ifstream file;
		file.open( Global::BaMMpatternFilename, std::ifstream::in );
		std::string pattern;
		int length;
		if( !file.good() ){
			std::cout << "Error: Cannot open bindingSitesFile sequence file: "
					<< Global::BaMMpatternFilename << std::endl;
			exit( -1 );
		} else {
			while( getline( file, pattern ) ){

				length = ( int ) pattern.length();			// get length of the first sequence

				Motif* motif = new Motif( length );

				motif->initFromBaMMPattern( pattern );

				motifs_.push_back( motif );
			}
		}

	} else if( Global::bindingSiteFilename != NULL ){

		std::ifstream file;
		file.open( Global::bindingSiteFilename, std::ifstream::in );
		std::string bindingSite;
		int length;

		if( !file.good() ){
			std::cout << "Error: Cannot open bindingSitesFile sequence file: "
					<< Global::bindingSiteFilename << std::endl;
			exit( -1 );
		} else {
			getline( file, bindingSite );					// get length of the first sequence
			length = ( int ) bindingSite.length();
		}

		length += Global::addColumns.at( 0 ) + Global::addColumns.at( 1 );

		Motif* motif = new Motif( length );

		motif->initFromBindingSites( Global::bindingSiteFilename );

		motifs_.push_back( motif );

		N_ = 1;

	} else if( Global::PWMFilename != NULL ){

		// read file to calculate motif length
		// Motif* motif = new Motif( length )
		// motif.initFromPWM( file )
		// motifs.push_back( motif )

	} else if( Global::BaMMFilename != NULL ){

		// read file to calculate motif length
		// Motif* motif = new Motif( length )
		// motif.initFromBMM( file )
		// motifs.push_back( motif )
	}

//	if( Global::verbose ) print();
}

MotifSet::~MotifSet(){
    for( size_t i = 0; i < motifs_.size(); i++ ){
        delete motifs_[i];
    }
}

std::vector<Motif*> MotifSet::getMotifs(){
    return motifs_;
}

int MotifSet::getN(){
    return N_;
}

void MotifSet::print(){
	printf( " ____________________________\n"
			"|                            |\n"
			"| PROBABILITIES for MotifSet |\n"
			"|____________________________|\n\n" );

	for( int i = 0; i < N_; i++ ){
		printf( " ________________________________________\n"
				"|                                        |\n"
				"| INITIALIZED PROBABILITIES for Motif %d  |\n"
				"|________________________________________|\n\n", i+1 );
		motifs_[i]->print();
	}
}
void MotifSet::write(){

}
