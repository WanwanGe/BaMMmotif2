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
                        GFdr::negSequenceSet->getBaseFrequencies(),
                        GFdr::modelOrder,
                        GFdr::modelAlpha,
                        GFdr::maxPWM,
                        GFdr::q );

    /**
     * Generate negative sequence set for cross-validation
     */
    std::vector<Sequence*>  negset;
    size_t posN = GFdr::posSequenceSet->getSequences().size();
    if( GFdr::B3 ){
        // take the given negative sequence set
        negset = GFdr::negSequenceSet->getSequences();

    } else {
        // generate negative sequence set based on s-mer frequencies
        // from positive training sequence set
        std::vector<std::unique_ptr<Sequence>> negSeqs;
        SeqGenerator negseq(GFdr::posSequenceSet->getSequences(), NULL, GFdr::sOrder);
        if( !GFdr::fixedNegN and posN >= GFdr::negN ){
            negSeqs = negseq.sample_bgseqset_by_fold(GFdr::mFold);
            std::cout << posN << " x "<< GFdr::mFold << " background sequences are generated." << std::endl;
        } else if( !GFdr::fixedNegN and posN < GFdr::negN ){
            GFdr::mFold = GFdr::negN / posN + 1;
            negSeqs = negseq.sample_bgseqset_by_fold(GFdr::mFold);
            std::cout << posN << " x "<< GFdr::mFold << " background sequences are generated." << std::endl;
        } else {
            negSeqs = negseq.sample_bgseqset_by_num(GFdr::negN, GFdr::posSequenceSet->getMaxL());
            std::cout << GFdr::negN << " (fixed) background sequences are generated." << std::endl;
        }
        // convert unique_ptr to regular pointer
        for (size_t n = 0; n < negSeqs.size(); n++) {
            negset.push_back(negSeqs[n].release());
            negSeqs[n].get_deleter();
        }
    }

    /**
     * Optional: subsample input sequence set when it is two large
     */
    std::vector<Sequence*> posset;
    if(!GFdr::fixedPosN){
        posset = GFdr::posSequenceSet->getSequences();
    } else {
        size_t posNsub = std::min( GFdr::maxPosN, posN );
        std::vector<size_t> indices(posN);
        std::iota(indices.begin(), indices.end(), 0);
        std::random_shuffle(indices.begin(), indices.end());
        for(size_t n = 0; n < posNsub; n++){
            posset.push_back(GFdr::posSequenceSet->getSequences()[indices[n]]);
        }
    }

    /**
     * Cross-validate the motif model
     */
#pragma omp parallel for
    for( size_t n = 0; n < motif_set.getN(); n++ ){

        Motif* motif = new Motif( *motif_set.getMotifs()[n] );

        // make sure the motif length does not exceed the sequence length
        size_t minPosL = ( GFdr::ss ) ? GFdr::posSequenceSet->getMinL() : GFdr::posSequenceSet->getMinL() * 2;
        size_t minNegL = ( GFdr::ss ) ? GFdr::negSequenceSet->getMinL() : GFdr::negSequenceSet->getMinL() * 2;
        assert( motif->getW() <= minPosL );
        assert( motif->getW() <= minNegL );

        FDR fdr( posset, negset,
                 motif, bgModel,
                 GFdr::cvFold, GFdr::mops, GFdr::zoops,
                 true, GFdr::savePvalues, GFdr::saveLogOdds );

        fdr.evaluateMotif( GFdr::EM, GFdr::CGS );

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

    for (size_t n = 0; n < negset.size(); n++) {
        if( !GFdr::B3 and negset[n] ) delete negset[n];
    }

    // free memory
    if( bgModel ) delete bgModel;

    GFdr::destruct();

    return 0;
}