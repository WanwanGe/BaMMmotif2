#ifndef SEQGENERATOR_H_
#define SEQGENERATOR_H_

#include "Alphabet.h"
#include "BackgroundModel.h"
#include "utils.h"
#include "Motif.h"
#include "ModelLearning.h"
#include "Global.h"

class SeqGenerator {

	/*
	 * This class generates artificial sequences set for:
	 * 1. negative sequence set, by using k-mer frequencies;
	 * 2. simulated positive sequence set by inserting motif
	 *    into the negative sequence set
	 * Prerequisite:
	 * 1. sequences set for calculating k-mer frequencies
	 * 2. the order of k-mer for sampling
	 * 3. (optional) the motif model for generating pseudo-sequence set
	 */

public:
	SeqGenerator( std::vector<Sequence*> seqs,
					Motif* motif = NULL,
					size_t sOrder = 2 );
	~SeqGenerator();

	std::vector<std::unique_ptr<Sequence>> generate_negative_seqset( size_t fold );
	void generate_seqset_with_embedded_motif( size_t fold );

	void write_seqset_with_embedded_motif( char* odir, std::string basename );

private:

	void						calculate_kmer_frequency();
	std::unique_ptr<Sequence> 	sample_negative_sequence( size_t L );
	std::unique_ptr<Sequence> 	sample_pseudo_sequence( size_t L );

	std::vector<Sequence*> 		seqs_;			// positive sequence set
	float**						freqs_;			// k-mer frequencies
	size_t** 					count_;			// k-mer counts
	Motif* 						motif_;			// the optimized motif
	size_t						sOrder_;		// the order of k-mers for
												// generating negative/pseudo
												// sequence set
	std::vector<std::unique_ptr<Sequence>> pseudo_posset_;

	std::vector<size_t>			Y_;
};

#endif /* SEQGENERATOR_H_ */
