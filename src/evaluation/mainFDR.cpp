//
// Created by wanwan on 03.09.17.
//

#include "GFdr.h"
#include "FDR.h"

int main( int nargs, char* args[] ){

    /**
     * initialization
     */

	GFdr::init( nargs, args );

    /**
     * Build up the background model
     */
	BackgroundModel* bgModel;
    if( GFdr::bgModelFilename == NULL ) {
        bgModel = new BackgroundModel(GFdr::posSequenceSet->getSequences(),
                                      GFdr::bgModelOrder,
                                      GFdr::bgModelAlpha,
                                      GFdr::interpolateBG,
                                      GFdr::outputFileBasename);
    } else {
        bgModel = new BackgroundModel( GFdr::bgModelFilename );
    }

    if(GFdr::saveInitialModel){
        // save background model
        bgModel->write(GFdr::outputDirectory, GFdr::outputFileBasename);
    }

    /**
     * Initialize the model
     */
    MotifSet motif_set( GFdr::initialModelFilename,
                        GFdr::addColumns.at(0),
                        GFdr::addColumns.at(1),
                        GFdr::initialModelTag,
                        GFdr::posSequenceSet,
                        bgModel->getV(),
                        GFdr::bgModelOrder,
                        GFdr::modelOrder,
                        GFdr::modelAlpha,
                        GFdr::maxPWM,
                        GFdr::q );

    /**
     * Filter out short sequences
     */
    std::vector<Sequence*> posSet = GFdr::posSequenceSet->getSequences();
    size_t count = 0;
    std::vector<Sequence*>::iterator it = posSet.begin();
    while( it != posSet.end() ){
        if( (*it)->getL() < motif_set.getMaxW() ){
            //std::cout << "Warning: remove the short sequence: " << (*it)->getHeader() << std::endl;
            posSet.erase(it);
            count++;
        } else {
            it++;
        }
    }
    if( count > 0 ){
        std::cout << "Note: " << count << " short sequences have been filtered out." << std::endl;
    }

    /**
     * Optional: subsample input sequence set when it is too large
     */
    size_t posN = posSet.size();
    if( GFdr::fixedPosN and GFdr::maxPosN < posN ){
        std::random_shuffle(posSet.begin(), posSet.end());
        for( size_t n = posN-GFdr::maxPosN; n > 0; n-- ){
            posSet.erase( posSet.begin() + GFdr::maxPosN + n - 1 );
        }
    }

    /**
     * Generate negative sequence set for cross-validation
     */
    std::vector<Sequence*>  negset;
    if( GFdr::B3 ){
        // take the given negative sequence set
        negset = GFdr::negSequenceSet->getSequences();

    } else {
        // generate negative sequence set based on s-mer frequencies
        // from positive training sequence set
        posN = posSet.size();   // update the size of positive sequences after filtering
        std::vector<std::unique_ptr<Sequence>> negSeqs;
        SeqGenerator negseq( posSet, NULL, GFdr::sOrder, GFdr::q, GFdr::genericNeg );
        if( !GFdr::fixedNegN and posN >= GFdr::negN ){
            negSeqs = negseq.sample_bgseqset_by_fold( GFdr::mFold );
            std::cout << GFdr::mFold << " x " << posN << " background sequences are generated." << std::endl;
        } else if( !GFdr::fixedNegN and posN < GFdr::negN ){
            bool rest = GFdr::negN % posN;
            GFdr::mFold = GFdr::negN / posN + rest;
            negSeqs = negseq.sample_bgseqset_by_fold( GFdr::mFold );
            std::cout << GFdr::mFold << " x " << posN << " background sequences are generated." << std::endl;
        } else {
            negSeqs = negseq.sample_bgseqset_by_num( GFdr::negN, GFdr::posSequenceSet->getMaxL() );
            std::cout << GFdr::negN << " (fixed) background sequences are generated." << std::endl;
        }

        // convert unique_ptr to regular pointer
        for (size_t n = 0; n < negSeqs.size(); n++) {
            negset.push_back(negSeqs[n].release());
            negSeqs[n].get_deleter();
        }
    }

    /**
     * Cross-validate the motif model
     */
    // Determine whether to optimize over motifs
    // or over optimization steps within a motif
    size_t mainLoopThreads;
    size_t perLoopThreads;
    if( GFdr::parallel_motif ){
        mainLoopThreads = GFdr::threads;
        perLoopThreads = 1;
    } else {
        mainLoopThreads = 1;
        perLoopThreads = GFdr::threads;
    }

#pragma omp parallel for num_threads( mainLoopThreads )

    for( size_t n = 0; n < motif_set.getN(); n++ ){

        Motif* motif = new Motif( *motif_set.getMotifs()[n] );

        FDR fdr( posSet, negset, GFdr::ss,
                 motif, bgModel,
                 GFdr::cvFold, GFdr::mops, GFdr::zoops,
                 true, GFdr::savePvalues, GFdr::saveLogOdds );

        fdr.evaluateMotif( GFdr::EM, GFdr::CGS, false, false, false,
                           0.05f, perLoopThreads );

        if(GFdr::saveInitialModel){
            // write out the foreground model
            motif->write( GFdr::outputDirectory,
                          GFdr::outputFileBasename + "_init_motif_" + std::to_string( n+1 ) );
        }

        std::string fileExtension;
        if( GFdr::initialModelTag == "PWM" ) {
            fileExtension = "_motif_" + std::to_string(n + 1);
        }

        fdr.write( GFdr::outputDirectory,
                   GFdr::outputFileBasename + fileExtension );
        if( motif )		delete motif;
    }

    if( !GFdr::B3 and negset.size() ) {
        for (size_t n = 0; n < negset.size(); n++) {
            delete negset[n];
        }
    }

    // free memory
    if( bgModel ) delete bgModel;

    GFdr::destruct();

    return 0;
}