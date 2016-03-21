# assembly

cc assemble_reads_0315.c -o assemble_reads_0315

./assemble_reads_0315 reads.file.fasta kmer.list.file assembly.segments.fasta assembly.genome.fasta reindex.reads.fasta lowest_cov kmer_size > assembly.info
