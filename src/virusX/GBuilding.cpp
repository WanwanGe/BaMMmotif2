#include "GBuilding.h"

char*               Global::inputDirectory = NULL;				// input directory
char*               Global::outputDirectory = NULL;				// output directory

char*				Global::extension = NULL;					// extension of files in FASTA format, defaults to fasta

size_t				Global::modelOrder = 2;						// background model order
std::vector<float>	Global::modelAlpha( modelOrder+1, 1.0f );	// background model alpha
float				Global::modelBeta = 10.0f;					// alpha_k = beta x gamma^(k-1) for k > 0
float				Global::modelGamma = 2.0f;					// - " -

bool				Global::interpolate = true;					// calculate prior probabilities from lower-order probabilities
																// instead of background frequencies of mononucleotides

bool                Global::verbose = false;					// verbose printouts

std::string			Global::name="build";						// program name
std::string			Global::version="0.1.1";					// program version number

void Global::init( int nargs, char* args[] ){

	readArguments( nargs, args );

	char* alphabetString = strdup( "STANDARD" );
	Alphabet::init( alphabetString );
	free( alphabetString );
}

void Global::destruct(){

	if( inputDirectory != NULL ){
		free( inputDirectory );
	}
	if( outputDirectory != NULL ){
		free( outputDirectory );
	}
	if( extension != NULL ){
		free( extension );
	}

	Alphabet::destruct();
}

int Global::readArguments( int nargs, char* args[] ){

	GetOpt::GetOpt_pp opt( nargs, args );

	if( opt >> GetOpt::OptionPresent( 'h', "help" ) ){
		printHelp();
		exit( -1 );
	}

	if( opt >> GetOpt::OptionPresent( "version" ) ){
		std::cout << name << " version " << version << std::endl;
		exit( -1 );
	}

	if( opt >> GetOpt::OptionPresent( 'e', "extension" ) ){
		opt >> GetOpt::Option( 'e', "extension", extension );
	} else {
		extension = strdup( "fasta" );
	}

	opt >> GetOpt::Option( 'k', modelOrder );

	if( opt >> GetOpt::OptionPresent( 'a', "alpha" ) ){
		modelAlpha.clear();
		opt >> GetOpt::Option( 'a', "alpha", modelAlpha );
		if( modelAlpha.size() != modelOrder+1 ){
			if( modelAlpha.size() > modelOrder+1 ){
				modelAlpha.resize( modelOrder+1 );
			} else{
				modelAlpha.resize( modelOrder+1, modelAlpha.back() );
			}
		}
		float b;
		if( opt >> GetOpt::OptionPresent( 'b', "beta" ) ){
			opt >> GetOpt::Option( 'b', "beta", b );
			fprintf( stderr, "Option -b is ignored.\n" );
		}
		float g;
		if( opt >> GetOpt::OptionPresent( 'g', "gamma" ) ){
			opt >> GetOpt::Option( 'g', "gamma", g );
			fprintf( stderr, "Option -g is ignored.\n" );
		}
	} else{
		if( opt >> GetOpt::OptionPresent( 'b', "beta" ) ){
			opt >> GetOpt::Option( 'b', "beta", modelBeta );
		}
		if( opt >> GetOpt::OptionPresent( 'g', "gamma" ) ){
			opt >> GetOpt::Option( 'g', "gamma", modelGamma );
		}
		if( modelAlpha.size() != modelOrder+1 ){
			if( modelAlpha.size() > modelOrder+1 ){
				modelAlpha.resize( modelOrder+1 );
			} else{
				modelAlpha.resize( modelOrder+1, modelAlpha.back() );
			}
		}
		if( modelOrder > 0 ){
			for( unsigned int i=1; i < modelAlpha.size(); i++ ){
				modelAlpha[i] = modelBeta * powf( modelGamma, static_cast<float>( i )-1.0f );
			}
		}
	}

	if( opt >> GetOpt::OptionPresent( "nonBayesian" ) ){
		interpolate = false;
	}

	if( opt >> GetOpt::OptionPresent( 'o', "outputDirectory" ) ){
		opt >> GetOpt::Option( 'o', "outputDirectory", outputDirectory );
		struct stat sb;
		if( stat( outputDirectory, &sb ) != 0 || !( S_ISDIR( sb.st_mode ) ) ){
			std::cerr << "Error: Output directory is not a directory" << std::endl;
		}
	}


	opt >> GetOpt::OptionPresent( 'v', "verbose", verbose );

	std::vector<std::string> argv;
	opt >> GetOpt::GlobalOption( argv );
	if( !( argv.size() == 1 ) ){
		std::cerr << "Error: Input directory with genomes in FASTA format missing" << std::endl;
		printHelp();
		exit( -1 );
	}
	inputDirectory = strdup( argv[0].c_str() );
	struct stat sb;
	if( stat( inputDirectory, &sb ) != 0 || !( S_ISDIR( sb.st_mode ) ) ){
		std::cerr << "Error: Input directory is not a directory" << std::endl;
	}

	if( outputDirectory == NULL ){
		outputDirectory = strdup( inputDirectory );
	}

	if( opt.options_remain() ){
		std::cerr << "Warning: Unknown arguments ignored" << std::endl;
	}

	return 0;
}

void Global::printHelp(){

	bool virusHostHelp = true; // print virus-host interaction or rather unspecific help comments
	bool developerHelp = false; // print developer-specific options

	printf( "\n" );
	printf( "SYNOPSIS\n" );
	printf( "      build DIRPATH [OPTIONS]\n\n" );
	printf( "DESCRIPTION\n" );
	if( virusHostHelp ){
		printf( "      Learn homogeneous Bayesian Markov models (BaMMs) from bacterial genomes.\n\n" );
		printf( "      DIRPATH\n"
				"          Input directory with bacterial genomes in FASTA format (one file per\n"
				"          genome). The default filename extension searched for is <fasta>. Use\n"
				"          option -e (--extension) to change the default setting.\n\n" );
	} else{
		printf( "      Learn homogeneous Bayesian Markov models (BaMMs) from sequence sets.\n\n" );
		printf( "      DIRPATH\n"
				"          Input directory with sequence sets in FASTA format. The default\n"
				"          filename extension searched for is <fasta>. Use option -e\n"
				"          (--extension) to change the default setting.\n\n" );
	}
	printf( "OPTIONS\n" );
	printf( "  Options for input files\n" );
	printf( "      -e, --extension <STRING>\n"
			"          The filename extension of FASTA files searched for in the input\n"
			"          directory. The default is <fasta>.\n\n" );
	printf( "  Options for Bayesian homogeneous Markov models\n" );
	printf( "      -k, --order <INTEGER>\n"
			"          Order. The default is 2.\n\n" );
	printf( "      -a, --alpha <FLOAT> [<FLOAT>...]\n"
			"          Order-specific prior strength. The default is 1.0 (for k = 0) and\n"
			"          20 x 3^(k-1) (for k > 0). The options -b and -g are ignored.\n\n" );
	printf( "      -b, --beta <FLOAT>\n"
			"          Calculate order-specific alphas according to beta x gamma^(k-1) (for\n"
			"          k > 0). The default is 20.0.\n\n" );
	printf( "      -g, --gamma <FLOAT>\n"
			"          Calculate order-specific alphas according to beta x gamma^(k-1) (for\n"
			"          k > 0). The default is 3.0.\n\n" );
	printf( "      --nonBayesian\n"
			"          Calculate prior probabilities from background frequencies of\n"
			"          mononucleotides instead of lower-order probabilities.\n\n" );
	printf( "  Output options\n" );
	printf( "      -o, --outputDirectory <DIRPATH>\n"
			"          Output directory for Bayesian homogenous Markov models. The input\n"
			"          directory is the default output directory.\n\n" );
	printf( "      -v, --verbose\n"
			"          Verbose terminal printouts.\n\n" );
	printf( "      -h, --help\n"
			"          Printout this help.\n\n" );
	printf( "      --version\n"
			"          Show program name/version banner.\n\n" );
	if( developerHelp ){
		printf( "      (*) Developer options\n\n" );
	}
}
