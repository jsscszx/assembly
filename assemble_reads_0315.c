#include <stdio.h>   /* required for file operations */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define Length 50000  //max read length
#define ReadNumber 50000 // max read number
#define KmerSize 25 // max kmer size
#define MinOverlap 8000 // minOverlap length
#define MaxOverhang 1500 // maxOverhang length
#define Jump 1500 // jump parameter


// for test purposes
#define CHECK_TAG 0
#define XXX1 41278
#define XXX2 40210
#define XXX3 18480
#define XXX4 20672


// The Table of All Frequent kmers
struct kmer_state{ 
	int cov; // the number of appearences 
	char seq[KmerSize]; // kmer sequence e.g. ATCCCGG
	int reads[50]; // the read_index that this k-mer appears
	int reads_pos[50]; // the read_position that this k-mer appears
	int true_cov; // the downsampled number of appearences 
};

struct read_state{
	char nc[Length]; //sequence
	int len; // length
	int kmer_index[25000]; // list of kmers
	int kmer_pos[25000]; // list of kmer_positions
	int kmer_count; // number of kmers
	int chimeric_tag; // whether it is chimeric (=1) or not (=0), 
	int overlap_read[200]; // overlapping reads
	int overlap_start_pos1[200]; // start position of the overlapped region in the overlapping reads
	int overlap_end_pos1[200]; // end position of the overlapped region in the overlapping reads
        int overlap_start_pos2[200]; // start position of the overlapped region in this read
        int overlap_end_pos2[200]; // end position of the overlapped region in this read
	int overlap_read_count; // number of overlapping reads
	int visited_tag; // whether it has been visited (=1) or not (=0)
};


//Init kmer table
int init_kmer(struct kmer_state * kmer, int kmer_number){
	int i;
	for (i = 0; i <kmer_number; i++){
		kmer[i].cov = 0;
		kmer[i].reads[0] = -1;
		kmer[i].reads_pos[0] = -1;
		kmer[i].true_cov = 0;
		kmer[i].seq[0] = '\0';
	}
}

//Init read table
int init_reads(struct read_state * Reads, int read_number){
	int i;
	for (i = 0; i < read_number; i++){
		Reads[i].nc[0] = '\0';
		Reads[i].kmer_count = 0;
		Reads[i].kmer_index[0] = -1;
		Reads[i].kmer_pos[0] = -1;
		Reads[i].chimeric_tag = 1;
		Reads[i].overlap_read_count = 0;
		Reads[i].visited_tag = 0;
	}
}


//Init hash table: use the first 10 character of kmer as the hash index, 4^10 < 1050000
int init_hashtable(int* hashtable){
	int i;
	for (i = 0; i < 1050000; i++){
		hashtable[i] = -1;
	}
}

// Get the index from a range [index1,index2] in the kmer table, using binary search
int get_index(char * seq, struct kmer_state * kmer, int index1, int index2){
	int lower, upper, middle;
	lower = index1;
	upper = index2;
	while(lower < upper){
		middle = (lower+upper)/2;
		//printf("seq %s upper(%d) %s middle(%d) %s lower(%d) %s", seq, upper, kmer[upper].seq, middle, kmer[middle].seq, lower, kmer[lower].seq); getchar();
		if (strcmp(seq, kmer[middle].seq) < 0){
			upper = middle-1;
		}else if (strcmp(seq, kmer[middle].seq) > 0){
			lower = middle+1;
		}else if  (strcmp(seq, kmer[middle].seq) == 0){
			return(middle);
		}
	}
	if  (strcmp(seq, kmer[lower].seq) == 0) 
		return(lower);
	else if  (strcmp(seq, kmer[upper].seq) == 0) 
		return(upper);
	return(-1);
}


//naive implementation of returning the median value of an array of count numbers 
int get_median(int * array, int count){
	int i, j;
	int pos;
	int swap;
   	for (i= 0;i<count-1;i++){
      		pos = i;
		for (j=i+1;j<count;j++){
			if (array[pos]>array[j])
			    pos=j;
	  	}
		if (pos != i){
			swap = array[i];
			array[i] = array[pos];
			array[pos] = swap;
		}
	}
	return(array[count/2]);
}


//get the hash index from a kmer, index the first 10 characters of a kmer to be a number in [0, 4^10-1]
int get_hash_index(char *seq){
	int index = 0;
	int i;
	for (i = 0; i < 10 ; i++){
		if (seq[i] == 'A')
			index = index * 4;
		else if (seq[i] == 'C')
			index = index * 4+1;
		else if (seq[i] == 'G')
			index = index * 4+2;
		else if (seq[i] == 'T')
			index = index * 4+3;
	}
	return(index);
}

// return the abosolute difference of a and b
int abs_dis(int a, int b){
	if (a - b >= 0)
		return(a - b);
	else
		return(b - a);

}


// test if the j-th overlapping read of read i extends to the right wrt read i, and return 1 if yes otherwise 0
int check_right_extend(int i, int j, struct read_state * Reads){
	int temp_read;
	temp_read = Reads[i].overlap_read[j];
	if ((Reads[temp_read].len - Reads[i].overlap_end_pos2[j]) > (Reads[i].len - Reads[i].overlap_end_pos1[j]))
		return(1);
	else
		return(0);
}

// check how many (non-chimeric) reads may extend to the right from read i, return that numner
int right_extend_number(int i, struct read_state * Reads){
        int temp_read;
        int j;
	int count = 0;
	for (j = 0; j < Reads[i].overlap_read_count ; j++){
		temp_read = Reads[i].overlap_read[j];
		if ((check_right_extend(i, j, Reads) == 1)&&(Reads[temp_read].chimeric_tag == 0)){
			count++;
		}
	}


}


// from the current read i, find a read that extends i to the right and return its index (as candidate), if i overlaps with the start read, then return the start read
int move_right_read(int i, struct read_state * Reads, int start_read){
        int j, j1,j2, j3;
	int candidate = -1 ;
	int temp_read;
	int max_overlap = -1;
        if (Reads[i].chimeric_tag >= 0){
                for (j1 = 0; j1 < Reads[i].overlap_read_count ; j1++){
                        temp_read = Reads[i].overlap_read[j1];
                        if ((temp_read == start_read)&&(start_read != i)&&(Reads[start_read].len - Reads[i].overlap_end_pos2[j1] > Reads[i].len - Reads[i].overlap_end_pos1[j1] )){
				candidate = start_read;
				printf("read[%d].overlap[%d]=%d \n", i,j1, temp_read);
				return(candidate);
			}
			if ((check_right_extend(i, j1, Reads) == 1)&&(Reads[temp_read].chimeric_tag == 0) && (right_extend_number(temp_read, Reads) > 0 )){
				if (Reads[i].overlap_end_pos1[j1] - Reads[i].overlap_start_pos1[j1] > max_overlap){
					max_overlap = Reads[i].overlap_end_pos1[j1] - Reads[i].overlap_start_pos1[j1];
					candidate = temp_read;
				}	
			}		
                }
		if (candidate == -1){
			for (j1 = 0; j1 < Reads[i].overlap_read_count ; j1++){
				temp_read = Reads[i].overlap_read[j1];
				if ((check_right_extend(i, j, Reads) == 1)&&(Reads[temp_read].chimeric_tag >= 0) && (right_extend_number(temp_read, Reads) >= 0 )){
					if (Reads[i].overlap_end_pos1[j1] - Reads[i].overlap_start_pos1[j1] > max_overlap){
						max_overlap = Reads[i].overlap_end_pos1[j1] - Reads[i].overlap_start_pos1[j1];
						candidate = temp_read;
					}
				}
			}
		}

        }
	return(candidate);
}


// computing the jumping position from Reads[current] to Reads[next], find out that Reads[current] pos x (=jump_from) aligns to Reads[next] pos y(=jump_to),  x > prev_jump in Reads[current] 
int get_jumping_index(int current, int next, struct read_state * Reads, struct kmer_state * kmer, int *jump_from, int *jump_to, int prev_jump){
	int j1,j;
        int array_median[50000];
        int array_count=0;
	int dis_median;
	int temp_kmer_index;
	int prev_pos = -100;
	int diff = 100000;

        for (j = 0; j < Reads[current].overlap_read_count ; j++){
		if (Reads[current].overlap_read[j] == next){
			diff = (Reads[current].overlap_start_pos1[j] - Reads[current].overlap_start_pos2[j] + Reads[current].overlap_end_pos1[j] - Reads[current].overlap_end_pos2[j])/2;
		}
	}
	if (diff == 100000){
		for (j = 0; j < Reads[next].overlap_read_count ; j++){
			if (Reads[next].overlap_read[j] == current){
				diff = -(Reads[next].overlap_start_pos1[j] - Reads[next].overlap_start_pos2[j] + Reads[next].overlap_end_pos1[j] - Reads[next].overlap_end_pos2[j])/2;
			}
		}
	}

        for (j = 0; j < Reads[current].kmer_count ; j++){
                temp_kmer_index = Reads[current].kmer_index[j];
                if (kmer[temp_kmer_index].true_cov > 0){
                        for (j1 = 0; j1 < kmer[temp_kmer_index].true_cov ; j1++){
                                if (kmer[temp_kmer_index].reads[j1] == next){
                                        if (Reads[current].kmer_pos[j] > prev_jump){
						if (((Reads[current].kmer_pos[j] - kmer[temp_kmer_index].reads_pos[j1] > diff-Jump/2)&&(Reads[current].kmer_pos[j] - kmer[temp_kmer_index].reads_pos[j1] < diff+Jump/2))||(diff == 100000)){
							array_median[array_count] = Reads[current].kmer_pos[j] - kmer[temp_kmer_index].reads_pos[j1];
							array_count++;
							prev_pos = Reads[current].kmer_pos[j];
						}
					}
					printf("LINK [%d] [%d]: %d-%d=%d (%d) \n", current,next, Reads[current].kmer_pos[j], kmer[temp_kmer_index].reads_pos[j1], Reads[current].kmer_pos[j]- kmer[temp_kmer_index].reads_pos[j1], diff);
                                }
                        }
                }
        }
	dis_median = get_median(array_median, array_count);
        for (j = 0; j < Reads[current].kmer_count ; j++){
                temp_kmer_index = Reads[current].kmer_index[j];
                if (kmer[temp_kmer_index].true_cov > 0){
                        for (j1 = 0; j1 < kmer[temp_kmer_index].true_cov ; j1++){
                                if (kmer[temp_kmer_index].reads[j1] == next){
                                        if ((Reads[current].kmer_pos[j] > prev_jump) ){
                                                if (abs_dis(dis_median, Reads[current].kmer_pos[j] - kmer[temp_kmer_index].reads_pos[j1]) < Jump/4){
							(*jump_from) = Reads[current].kmer_pos[j];
							(*jump_to) = kmer[temp_kmer_index].reads_pos[j1]; 
						        printf("MEDIAN [%d] [%d]: %d %d\n", current,next,(*jump_from), (*jump_to) );
							return(0);
						}
                                                
                                        }
                                }
                        }
                }
        }
}

// check Reads[i] and Reads[j] overlap or not, return 1 if they overlap and 0 otherwise
int check_overlap_read(int i, int j, struct read_state * Reads){
        int j1;
        int temp_read;
	if ((j == -1)||(i==-1))
		return(0);
        for (j1 = 0; j1 < Reads[i].overlap_read_count ; j1++){
                temp_read = Reads[i].overlap_read[j1];
                if (temp_read == j){
                        return(1);
                }
        }
        return(0);
}

//print the circular genome into genome_file, and print read segments into assembly_file 
int generate_circular_contig(int* assembled_reads,  struct read_state * Reads, struct kmer_state * kmer,char* assembly_file, char* genome_file){
	int j1;
	int start_read;
	int current_read;
	int next_read;
	int jump_from, jump_to;
	int start_read_right;
	int start_read_left;
	int prev_jump;
	int temp_read;
	int temp_read_left;
	int temp_read_right;
	int temp_read_pos;
	int temp_next_index;
	FILE *fw_assembly;
	FILE *fw_genome;
	fw_assembly = fopen(assembly_file, "w");
	fw_genome = fopen(genome_file, "w");
        fprintf(fw_genome, ">CircularGenome\n");
	j1 = 0;
	while (assembled_reads[j1]== -1){
		j1++;
	}
	start_read = assembled_reads[j1];
	current_read = assembled_reads[j1];
	prev_jump = 0.5* Reads[current_read].len;
	temp_next_index = j1+1;
        next_read = assembled_reads[temp_next_index];
	/*while(check_overlap_read(current_read, assembled_reads[temp_next_index+1], Reads) == 1){
                next_read = assembled_reads[temp_next_index+1];
		temp_next_index++;
	}*/
        get_jumping_index(current_read, next_read, Reads, kmer, &jump_from, &jump_to, prev_jump);
	printf("jump from Read %d from pos %d(%d) to Read %d pos %d(%d)\n", current_read, jump_from, Reads[current_read].len, next_read, jump_to, Reads[next_read].len);//getchar();
	start_read_right = jump_from;
	current_read = next_read;
	temp_read = next_read;
	temp_read_left = jump_to;
	prev_jump = jump_to;
	temp_next_index++;
        next_read = assembled_reads[temp_next_index];
	while (assembled_reads[temp_next_index] != -1){
		/*while(check_overlap_read(current_read, assembled_reads[temp_next_index+1], Reads) == 1){
			next_read = assembled_reads[temp_next_index+1];
			temp_next_index++;
		}*/	
		get_jumping_index(current_read, next_read, Reads, kmer, &jump_from, &jump_to, prev_jump);
		printf("jump from Read %d from pos %d(%d) to Read %d pos %d(%d)\n", current_read, jump_from, Reads[current_read].len, next_read, jump_to, Reads[next_read].len);//getchar();
		temp_read_right = jump_from;
		fprintf(fw_assembly, ">%d_%d\n", temp_read,temp_next_index);
		printf("print Read %d from pos %d to %d\n", temp_read, temp_read_left, temp_read_right);//getchar();
                for (temp_read_pos = temp_read_left; temp_read_pos < temp_read_right ; temp_read_pos++){
	                fprintf(fw_genome, "%c", Reads[temp_read].nc[temp_read_pos]);
	                fprintf(fw_assembly, "%c", Reads[temp_read].nc[temp_read_pos]);
                }
		fprintf(fw_assembly, "\n");
		temp_read = next_read;
                temp_read_left = jump_to;
	        prev_jump = jump_to;
		current_read = next_read;
		temp_next_index++;
        	next_read = assembled_reads[temp_next_index];
	}
        get_jumping_index(current_read, start_read, Reads, kmer, &jump_from, &jump_to, prev_jump);
	printf("jump from Read %d from pos %d(%d) to Read %d pos %d(%d)\n", current_read, jump_from, Reads[current_read].len, start_read, jump_to, Reads[start_read].len);//getchar();
        temp_read_right = jump_from;
	fprintf(fw_assembly, ">%d_last\n", temp_read);
	//printf("print Last Read %d from pos %d to %d\n", temp_read, temp_read_left, temp_read_right);//getchar();
	for (temp_read_pos = temp_read_left; temp_read_pos < temp_read_right ; temp_read_pos++){
        	fprintf(fw_genome, "%c", Reads[temp_read].nc[temp_read_pos]);
        	fprintf(fw_assembly, "%c", Reads[temp_read].nc[temp_read_pos]);
        }
	fprintf(fw_assembly, "\n");
	start_read_left = jump_to;
	fprintf(fw_assembly, ">%d_start\n", start_read);
	printf("print Start Read %d from pos %d to %d\n", start_read, start_read_left, start_read_right);//getchar();
	for (temp_read_pos = start_read_left; temp_read_pos < start_read_right ; temp_read_pos++){
                fprintf(fw_genome, "%c", Reads[start_read].nc[temp_read_pos]);
                fprintf(fw_assembly, "%c", Reads[start_read].nc[temp_read_pos]);
        }
	fprintf(fw_assembly, "\n");
	fclose(fw_genome);
	fclose(fw_assembly);
}

//test if two reads overlap or not, the first shared vertex (i1,j1), and the last shared vertex (i2,j2),  the first read has length len1, and the second read has length len2 
int test_overlay(int len1, int i1, int j1, int len2, int i2, int j2){
	int i;
	int j;
	if (j1-i1 < MinOverlap)
		return(0);
	if (i1 > i2)
		i = i2;
	else
		i = i1;
	if ((len1-j1) > (len2-j2))
		j = (len2-j2);
	else
		j = (len1-j1);
	if (((j1-i1) > (j2-i2)+Jump) || ((j1-i1) < (j2-i2) - Jump))
		return(0);
	if ((i>MaxOverhang)||(j > MaxOverhang)) 
		return(0);
	else
		return(1);
}


// test if the next position pair (n1,n2) can extend the current position pair (c1,c2) as a commom jump-path, where c1+jump>n1>c1, and c2+jump>n2>c2
// return 1 if (n1,n2) can not extend (c1,c2)
// return 0 if (c1,c2) can not be extended any more
// return 2 if (n1,n2) closely extends (c1,c2)
// return 3 if (n1,n2) loosely extends (c1,c2)
int test_jump(int c1, int n1, int c2, int n2){
	if (n1==c1)
		return(1);
	if ((n1-c1<0)||(n1-c1>Jump))
		return(0);
	if ((n1-c1<n2-c2+Jump/8)&&(n1-c1>n2-c2-Jump/8))
		return(2);
        if ((n1-c1<n2-c2+Jump/2)&&(n1-n1>n2-c2-Jump/2))
		return(3);
	return(1);

}


int left_i[ReadNumber][100];
int right_i[ReadNumber][100];
int left_j[ReadNumber][100];
int right_j[ReadNumber][100];

//find all overlapping reads for Reads[i] and check if Reads[i] is chimeric or not
int test_chimeric_read(int i, struct read_state * Reads, struct kmer_state * kmer, int reads_cov, int read_index){
	int j, j1, j2;
	int coverage[500];
	int chimeric[500];
	int active_count[ReadNumber];
	int temp_tag[100];
	int temp_length[100];
	int temp_max_length;
	int temp_min_length;
	int temp_max_j2;
	int temp_min_j2;
	int temp_include_tag;
	int linked[ReadNumber];
	int start_align_i[ReadNumber];
        int end_align_i[ReadNumber];
        int start_align_j[ReadNumber];
        int end_align_j[ReadNumber];
	int temp_kmer_index;
	int temp_other_read;
	int temp_i_loc, temp_j_loc;
	//int max_active[ReadNumber];	

	Reads[i].chimeric_tag = 0;
        Reads[i+1].chimeric_tag = 0;

	for (j = 0; j < 500 ; j++){
		chimeric[j] = 0;
		coverage[j] = 0;
	}
	for (j1 = 0; j1 < read_index ; j1++){
                active_count[j1] = 0;
		start_align_i[j1] = -1;
		end_align_i[j1] = -1;
                start_align_j[j1] = -1;
                end_align_j[j1] = -1;
		linked[j1] = 0;
	}

	for (j = 0; j < Reads[i].kmer_count ; j++){
		temp_kmer_index = Reads[i].kmer_index[j];
		temp_i_loc = Reads[i].kmer_pos[j];
		//printf("%d %d %s count %d\n", j, temp_kmer_index, kmer[temp_kmer_index].seq, kmer[temp_kmer_index].true_cov);getchar();
		if (kmer[temp_kmer_index].true_cov > 0){
			for (j1 = 0; j1 < kmer[temp_kmer_index].true_cov ; j1++){
				temp_other_read = kmer[temp_kmer_index].reads[j1];
				temp_j_loc = kmer[temp_kmer_index].reads_pos[j1];
				if (((i==XXX1)||(i==XXX2)||(i==XXX3)||(i==XXX4))||(CHECK_TAG))
 					printf("OVERLAP %d %d %d %d\n", i, temp_other_read, temp_i_loc, temp_j_loc); 
				if (active_count[temp_other_read] == 0){
					if (((temp_i_loc < MaxOverhang)&&(temp_j_loc < Reads[temp_other_read].len - MinOverlap + MaxOverhang))||((temp_j_loc < MaxOverhang)&&(temp_i_loc < Reads[i].len - MinOverlap))) {
						active_count[temp_other_read]++;
						left_i[temp_other_read][0] = temp_i_loc;
                                        	right_i[temp_other_read][0] = temp_i_loc;
                                        	left_j[temp_other_read][0] = temp_j_loc;
                                        	right_j[temp_other_read][0] = temp_j_loc;
					}
				}else{
					temp_max_length = -1;
					temp_min_length = 5000000;
					temp_include_tag = 0;
                                        for(j2 = 0; j2 < active_count[temp_other_read] ; j2++){
                                                temp_tag[j2] = test_jump(right_i[temp_other_read][j2], temp_i_loc, right_j[temp_other_read][j2], temp_j_loc);
	        	                        if (((i==XXX1)||(i==XXX2)||(i==XXX3)||(i==XXX4))&&(CHECK_TAG))
 							printf("OVERLAP %d %d [%d] %d-%d %d-%d Tag=%d\n", i, temp_other_read, j2, left_i[temp_other_read][j2],right_i[temp_other_read][j2], left_j[temp_other_read][j2],right_j[temp_other_read][j2], temp_tag[j2]);
                                                if (temp_tag[j2] == 2){
                                                        temp_length[j2] = temp_i_loc - left_i[temp_other_read][j2];
                                                        if (temp_length[j2] > temp_max_length){
                                                                temp_max_length = temp_length[j2];
								temp_max_j2 = j2;
							}
                                                }
	                                        temp_length[j2] = temp_i_loc - left_i[temp_other_read][j2];
                                                if (temp_length[j2] < temp_min_length){
                                                        temp_min_length = temp_length[j2];
                                                        temp_min_j2 = j2;
                                                }
                                        } 

	        	                if (((i==XXX1)||(i==XXX2)||(i==XXX3)||(i==XXX4))&&(CHECK_TAG))
                                                printf("OVERLAP %d %d max[%d] min[%d] total %d\n", i, temp_other_read, temp_max_j2, temp_min_j2, active_count[temp_other_read]);
					if (active_count[temp_other_read] >= 99){
						j2 = temp_min_j2;
						left_i[temp_other_read][j2] = left_i[temp_other_read][active_count[temp_other_read]-1];
						right_i[temp_other_read][j2] = right_i[temp_other_read][active_count[temp_other_read]-1];
						left_j[temp_other_read][j2] = left_j[temp_other_read][active_count[temp_other_read]-1];
						right_j[temp_other_read][j2] = right_j[temp_other_read][active_count[temp_other_read]-1];
						temp_tag[j2] = temp_tag[active_count[temp_other_read]-1];
						active_count[temp_other_read]--;
					}

                                        for(j2 = 0; j2 < active_count[temp_other_read] ; j2++){
                                                if (temp_tag[j2] == 2){
                                                        if (j2 ==  temp_max_j2){
								right_i[temp_other_read][j2] = temp_i_loc;
                                        			right_j[temp_other_read][j2] = temp_j_loc;
								temp_tag[j2] = 1;
								temp_include_tag = 1;
							}else {
								temp_tag[j2] = 0;
							}
                                                }else if (temp_tag[j2] == 3){
                                                                left_i[temp_other_read][active_count[temp_other_read]] = left_i[temp_other_read][j2];
                                                                left_j[temp_other_read][active_count[temp_other_read]] = left_j[temp_other_read][j2];
                                                                right_i[temp_other_read][active_count[temp_other_read]] = temp_i_loc;
                                                                right_j[temp_other_read][active_count[temp_other_read]] = temp_j_loc;
                                                                temp_tag[active_count[temp_other_read]] = 1;
                                                                active_count[temp_other_read]++;
                                                                temp_include_tag = 1;
						}
                                        }


                                        for(j2 = 0; j2 < active_count[temp_other_read] ; j2++){
                                                while((temp_tag[j2] == 0)&&(j2< active_count[temp_other_read])){
                                                        left_i[temp_other_read][j2] = left_i[temp_other_read][active_count[temp_other_read]-1];
                                                        right_i[temp_other_read][j2] = right_i[temp_other_read][active_count[temp_other_read]-1];
                                                        left_j[temp_other_read][j2] = left_j[temp_other_read][active_count[temp_other_read]-1];
                                                        right_j[temp_other_read][j2] = right_j[temp_other_read][active_count[temp_other_read]-1];
 							temp_tag[j2] = temp_tag[active_count[temp_other_read]-1];
							active_count[temp_other_read]--;
                                                }
                                        }

					if (temp_include_tag == 0){
						if (((temp_i_loc < MaxOverhang)&&(temp_j_loc < Reads[temp_other_read].len - MinOverlap + MaxOverhang))||((temp_j_loc < MaxOverhang)&&(temp_i_loc < Reads[i].len - MinOverlap))) {
	                                        	left_i[temp_other_read][active_count[temp_other_read]] = temp_i_loc;
        	                                	right_i[temp_other_read][active_count[temp_other_read]] = temp_i_loc;
                	                        	left_j[temp_other_read][active_count[temp_other_read]] = temp_j_loc;
                        	                	right_j[temp_other_read][active_count[temp_other_read]] = temp_j_loc;
							temp_tag[active_count[temp_other_read]]=1;
							active_count[temp_other_read]++;
                                                	if (active_count[temp_other_read] > 99){
                                                        	active_count[temp_other_read]--;
                                                	}
						}
					}else{
						for(j2 = 0; j2 < active_count[temp_other_read] ; j2++){
							if (((i==XXX1)||(i==XXX2)||(i==XXX3)||(i==XXX4))&&(CHECK_TAG))
								 printf("OVERLAP %d %d [%d] %d-%d %d-%d\n", i, temp_other_read, j2, left_i[temp_other_read][j2],right_i[temp_other_read][j2], left_j[temp_other_read][j2],right_j[temp_other_read][j2]);
							if( (test_overlay(Reads[i].len, left_i[temp_other_read][j2],right_i[temp_other_read][j2], Reads[temp_other_read].len, left_j[temp_other_read][j2],right_j[temp_other_read][j2])==1))  { 
								if (( (right_i[temp_other_read][j2]- left_i[temp_other_read][j2]) > end_align_i[temp_other_read]-start_align_i[temp_other_read]  )||(start_align_i[temp_other_read] == -1)){
									linked[temp_other_read] = 1;
									start_align_i[temp_other_read] = left_i[temp_other_read][j2];
									end_align_i[temp_other_read] = right_i[temp_other_read][j2];
									start_align_j[temp_other_read] = left_j[temp_other_read][j2];
									end_align_j[temp_other_read] = right_j[temp_other_read][j2];
								}else{
									linked[temp_other_read] = 0;
									active_count[temp_other_read] = 0;
								}	
							}
						}
					}
				}
			}
		}	
	}


        for (j1 = 0; j1 < read_index ; j1++){
                if ((j1 % 2 == 1)&&(linked[j1-1] == 1)){
                                ;
                }else{
                	if(linked[j1] ==1){
                        	for (j = start_align_i[j1] /100 + Jump/100; j < end_align_i[j1]/100 - Jump/100 ; j++){
                                	chimeric[j]++;
                                }
                                for (j = start_align_i[j1]/100; j <= end_align_i[j1]/100 ; j++){
                                	coverage[j]++;
                                }
				if (i %1000 == 0)
                                printf("Read[%d] (len %d left %d right %d right) Read[%d] (len %d left %d right %d) \n", i, Reads[i].len, start_align_i[j1], end_align_i[j1], j1, Reads[j1].len, start_align_j[j1], end_align_j[j1] );
                        }
                }
        }


	for (j = (MaxOverhang+Jump)/100; j < Reads[i].len/100 - (MaxOverhang+Jump)/100; j++){
		if (chimeric[j] <= 0.1*reads_cov){
			Reads[i].chimeric_tag = 1;
	                Reads[i+1].chimeric_tag = 1;
		}
	}

	if (Reads[i].chimeric_tag >= 0){
                if (i %1000 == 0)
		printf("Chimeric Read[%d] %d\n", i, Reads[i].chimeric_tag);
		for (j1 = 0; j1 < read_index ; j1++){
	                if ((j1 % 2 == 1)&&(linked[j1-1] == 1)){
        	                        ;
                	}else{
                        	if(linked[j1] == 1){
					if (Reads[i].overlap_read_count < 200){
						Reads[i].overlap_read[Reads[i].overlap_read_count] = j1;
        	                                Reads[i].overlap_start_pos1[Reads[i].overlap_read_count] =  start_align_i[j1];
                                                Reads[i].overlap_end_pos1[Reads[i].overlap_read_count] =  end_align_i[j1];
                                                Reads[i].overlap_start_pos2[Reads[i].overlap_read_count] =  start_align_j[j1];
                                                Reads[i].overlap_start_pos2[Reads[i].overlap_read_count] =  end_align_j[j1];
						Reads[i].overlap_read_count++;
                                                if (j1%2 == 0){
                                                        Reads[i+1].overlap_read[Reads[i+1].overlap_read_count] = j1+1;
	                                                Reads[i+1].overlap_start_pos1[Reads[i+1].overlap_read_count] = Reads[i].len - end_align_i[j1];
        	                                        Reads[i+1].overlap_end_pos1[Reads[i+1].overlap_read_count] = Reads[i].len - start_align_i[j1];
                	                                Reads[i+1].overlap_start_pos2[Reads[i+1].overlap_read_count] = Reads[j1+1].len - end_align_j[j1];
                        	                        Reads[i+1].overlap_start_pos2[Reads[i+1].overlap_read_count] = Reads[j1+1].len - start_align_j[j1];
                                                        Reads[i+1].overlap_read_count++;
                                                }else{
                                                        Reads[i+1].overlap_read[Reads[i+1].overlap_read_count] = j1-1;
                                                        Reads[i+1].overlap_start_pos1[Reads[i+1].overlap_read_count] = Reads[i].len - end_align_i[j1];
                                                        Reads[i+1].overlap_end_pos1[Reads[i+1].overlap_read_count] = Reads[i].len - start_align_i[j1];
                                                        Reads[i+1].overlap_start_pos2[Reads[i+1].overlap_read_count] = Reads[j1-1].len - end_align_j[j1];
                                                        Reads[i+1].overlap_start_pos2[Reads[i+1].overlap_read_count] = Reads[j1-1].len - start_align_j[j1];
                                                        Reads[i+1].overlap_read_count++;
                                                }
					}
                        	}
                	}
		}
	}
}


//read kmer file, the first line of the kmer file contains the number of (distinct) kmers as kmer_index (Note the total number of kmers would be (2*kmer_index) including reverse complementary pairs)
//for a pair of reverse complementary kmers kmer1 and kmer2, if kmer1 < kmer2 (dictionary order) then (the index of kmer2) = (the index of kmer1) + kmer_index
int read_kmer_file(char * kmer_file, struct kmer_state ** kmer, int* hashtable, int kmerlength, int * kmer_index1){
	FILE * fr_kmer;
	int file_end_tag;
        int temp_kmer_index, kmer_index, temp_hash_index, i;
	char temp_char[1000];
        char line[Length];
	fr_kmer = fopen(kmer_file, "r");	
	if (fr_kmer != NULL){			
		file_end_tag = 1;
		init_hashtable(hashtable);
		fgets(line, Length, fr_kmer);
		sscanf(line, "%s %d", temp_char, &kmer_index);
	        (*kmer) = (struct kmer_state *) malloc((kmer_index*2+2)*sizeof(struct kmer_state));
                init_kmer((*kmer), kmer_index);
		temp_kmer_index = 1;
		while((file_end_tag!=0)){ // read the reads file
			line[0] = 'X';
			if (fgets(line, Length, fr_kmer) == NULL){
				file_end_tag = 0;
			};
			if (line[0] == 'X')
				file_end_tag = 0;
			if (file_end_tag != 0){
				sscanf(line, "%s %d", (*kmer)[temp_kmer_index].seq, &(*kmer)[temp_kmer_index].cov);
				(*kmer)[temp_kmer_index].seq[kmerlength] = '\0';
				temp_hash_index = get_hash_index((*kmer)[temp_kmer_index].seq);
				if (hashtable[temp_hash_index] == -1){
					hashtable[temp_hash_index] = temp_kmer_index;
				}
                                for (i=0;i<kmerlength;i++){
					if ((*kmer)[temp_kmer_index].seq[kmerlength-1-i] == 'A') (*kmer)[temp_kmer_index+kmer_index].seq[i] = 'T';
                                        if ((*kmer)[temp_kmer_index].seq[kmerlength-1-i] == 'T') (*kmer)[temp_kmer_index+kmer_index].seq[i] = 'A';
                                        if ((*kmer)[temp_kmer_index].seq[kmerlength-1-i] == 'C') (*kmer)[temp_kmer_index+kmer_index].seq[i] = 'G';
                                        if ((*kmer)[temp_kmer_index].seq[kmerlength-1-i] == 'G') (*kmer)[temp_kmer_index+kmer_index].seq[i] = 'C';
				}
                                (*kmer)[temp_kmer_index+kmer_index].seq[kmerlength] = '\0';
                                (*kmer)[temp_kmer_index+kmer_index].cov = (*kmer)[temp_kmer_index].cov;
				temp_kmer_index++;
				if (temp_kmer_index % 10000 == -1)
					printf("Kmer %d %s count %d\n", temp_kmer_index-1, (*kmer)[temp_kmer_index-1].seq, (*kmer)[temp_kmer_index-1].cov);
			}			
		}
		hashtable[1048576] = kmer_index-1;
		fclose(fr_kmer);
	}
	(*kmer_index1) = kmer_index;
}

//read Reads file, first read the file once to get the number of reads as read_index, then allocate the memory for reads
int read_Reads_file(char *reads_file_input, char *reads_file_output, struct read_state ** Reads, int * read_index1){
        FILE *fr_reads;
	FILE *fw_reads;
	int file_end_tag;
	int read_index, i;
	char line[Length];
	fr_reads = fopen(reads_file_input, "r");	
	if (fr_reads != NULL){			
		file_end_tag = 1;
		read_index = 0;
		while(file_end_tag!=0){ // read the reads file
			line[0] = 'X';
			if (fgets(line, Length, fr_reads) == NULL){
				file_end_tag = 0;
			};
			if (line[0] == 'X')
				file_end_tag = 0;
			if (file_end_tag != 0){
				fgets(line, Length, fr_reads);
				read_index++;
			}			
		}
		fclose(fr_reads);
	}
	(*Reads) = (struct read_state *) malloc((read_index*2)*sizeof(struct read_state));

	fr_reads = fopen(reads_file_input, "r");	
	fw_reads = fopen(reads_file_output, "w");
	if (fr_reads != NULL){			
		file_end_tag = 1;
		init_reads((*Reads), read_index);
		read_index = 0;
		while(file_end_tag!=0){ // read the reads file
			line[0] = 'X';
			if (fgets(line, Length, fr_reads) == NULL){
				file_end_tag = 0;
			};
			if (line[0] == 'X')
				file_end_tag = 0;
			if (file_end_tag != 0){
				fgets(line, Length, fr_reads);
 				(*Reads)[read_index].len = strlen(line)-1;
				strcpy((*Reads)[read_index].nc, line);
				fprintf(fw_reads,">%d\n%s", read_index, (*Reads)[read_index].nc);
				(*Reads)[read_index].nc[(*Reads)[read_index].len] = '\0';
				read_index++;				
                                (*Reads)[read_index].len = (*Reads)[read_index-1].len;
                                (*Reads)[read_index].nc[(*Reads)[read_index].len] = '\0';
                                for (i=0;i<(*Reads)[read_index].len;i++){
                                	if (line[(*Reads)[read_index].len-1-i] == 'A') (*Reads)[read_index].nc[i] = 'T';
                                        if (line[(*Reads)[read_index].len-1-i] == 'T') (*Reads)[read_index].nc[i] = 'A';
                                        if (line[(*Reads)[read_index].len-1-i] == 'C') (*Reads)[read_index].nc[i] = 'G';
                                        if (line[(*Reads)[read_index].len-1-i] == 'G') (*Reads)[read_index].nc[i] = 'C';
                                }
                                read_index++;
				if (read_index % 100 == -1){
					printf("(*Read)[%d] len %d\n", read_index-1, (*Reads)[read_index-1].len);
				}
			}			
		}
		fclose(fr_reads);
		fclose(fw_reads);
	}
	(* read_index1) = read_index;
}

//Index reads from the kmer table
int indexing_Reads_kmer(struct read_state * Reads, struct kmer_state * kmer, int* hashtable, int read_index, int kmer_index, int kmerlength){
	int i, j, j1;
	int temp_hash_index, temp_table_index1, temp_table_index2;
        char temp_kmer1[KmerSize];
        char temp_kmer2[KmerSize];	
	int temp_gap, max_gap;
	int tag_overflow;
	int read_kmer_count;
	int temp_kmer_pos, temp_kmer_index;
	for (i = 0; i < read_index; i=i+2){
		//printf(" Reads[%d] len %d\n", i, Reads[i].len); getchar();
		read_kmer_count = 0;
		max_gap = 0;
		temp_gap = 0;
		for (j = 0; j < Reads[i].len-kmerlength-1 ; j++){
			for (j1 = j; j1 < j+kmerlength ; j1++){
				temp_kmer1[j1-j] = Reads[i].nc[j1];
				temp_kmer2[j+kmerlength-1-j1] = Reads[i+1].nc[Reads[i+1].len-1-j1];
			}
			temp_kmer1[kmerlength] = '\0';
                        temp_kmer2[kmerlength] = '\0'; 
			//printf(" Reads[%d] pos %d temp1 %s temp2 %s\n", i, j, temp_kmer1, temp_kmer2); //getchar();
			if (strcmp(temp_kmer1, temp_kmer2) < 0)
				temp_hash_index = get_hash_index(temp_kmer1);
			else
				temp_hash_index = get_hash_index(temp_kmer2);
			if (hashtable[temp_hash_index] == -1)
				temp_kmer_index = -1;
			else{
				temp_table_index1 = hashtable[temp_hash_index];
				temp_hash_index++;
				while(hashtable[temp_hash_index] == -1)
					temp_hash_index++;
				temp_table_index2 = hashtable[temp_hash_index];
				//printf("bounded by %s(%d) %s(%d)\n", kmer[temp_table_index1].seq,temp_table_index1, kmer[temp_table_index2].seq,temp_table_index2 ); getchar();
	                        if (strcmp(temp_kmer1, temp_kmer2) < 0)
					temp_kmer_index = get_index(temp_kmer1, kmer, temp_table_index1, temp_table_index2);
				else
					temp_kmer_index = get_index(temp_kmer2, kmer, temp_table_index1, temp_table_index2);
			}

			if (temp_kmer_index != -1){
				if (strcmp(temp_kmer1, temp_kmer2) > 0){
					temp_kmer_index = temp_kmer_index + kmer_index;
				}
				if (kmer[temp_kmer_index].cov < 24){
					tag_overflow = 0;
				}else if (kmer[temp_kmer_index].true_cov > 23){
					tag_overflow = 1;
				}else{
					if (rand() % kmer[temp_kmer_index].cov < 24){
						tag_overflow = 0;
					}else{
						tag_overflow = 1;
					}
				}
						
				if(tag_overflow == 0){
					if (kmer[temp_kmer_index].true_cov < 24){
						//kmer[temp_kmer_index].true_cov = 0;
						Reads[i].kmer_index[read_kmer_count] = temp_kmer_index;
						Reads[i].kmer_pos[read_kmer_count] = j;
						read_kmer_count++;
						kmer[temp_kmer_index].reads[kmer[temp_kmer_index].true_cov] = i;
						kmer[temp_kmer_index].reads_pos[kmer[temp_kmer_index].true_cov] = j;
						kmer[temp_kmer_index].true_cov++;
						temp_gap = 0;
					}
				}
			}else{
				temp_gap++;
				if (temp_gap > max_gap)
					max_gap = temp_gap;
			}
		}
		if (i %1000 == 0)
			printf("Reads[%d], kmer %d, gap %d\n", i, read_kmer_count, max_gap);
		Reads[i].kmer_count = read_kmer_count;
		Reads[i+1].kmer_count = read_kmer_count;
        	for (j = Reads[i].kmer_count -1; j>=0 ; j--){
                	temp_kmer_index = Reads[i].kmer_index[j];
			if (temp_kmer_index < kmer_index)
				temp_kmer_index = temp_kmer_index + kmer_index;
			else
				temp_kmer_index = temp_kmer_index - kmer_index;
			read_kmer_count = Reads[i].kmer_count -1 - j;
			temp_kmer_pos = Reads[i].len - Reads[i].kmer_pos[j] - kmerlength;
                        if (kmer[temp_kmer_index].true_cov < 24){
	                        Reads[i+1].kmer_index[read_kmer_count] = temp_kmer_index;
        	                Reads[i+1].kmer_pos[read_kmer_count] = temp_kmer_pos;
                                kmer[temp_kmer_index].reads[kmer[temp_kmer_index].true_cov] = i+1;
                                kmer[temp_kmer_index].reads_pos[kmer[temp_kmer_index].true_cov] = temp_kmer_pos;
                                kmer[temp_kmer_index].true_cov++;
                        } 

       		} 
	}
}


//assembe all reads, and write the assembly in terms of reads to file assembly_reads, and in terms of preassembled-genome to file assembly_genome
int assemble_reads(struct read_state * Reads, struct kmer_state * kmer, int read_index, int reads_cov, char* assembly_reads, char* assembly_genome){
	int j, i, i1;
	int start_read;
	int right_most_read;
	int assembled_reads[10000];

        j=0;
        while(j < read_index){ //find all overlapping reads w.r.t. each read j
                if (j%2 == 0)
                        test_chimeric_read(j, Reads, kmer,reads_cov, read_index);
                j++;
        }

        j=0;
        while(j < read_index){ 
                printf("Read[%d] -> Read[%d]  %d \n", j, move_right_read(j, Reads, 10), right_extend_number(j,Reads) );
                j++;
        }


        j = read_index % 1000;
        while((Reads[j].chimeric_tag == 1)||(right_extend_number(j,Reads) < 5)){
                j=j+1;
		printf("Try Read[%d] %d\n", j+1, right_extend_number(j+1,Reads));
        }
        j = move_right_read(j, Reads, j);
        start_read = j;
        start_read = j = 38880;
        Reads[j].visited_tag = 1;
        printf("Start Read %d\n", j);//getchar();
        for(i=0;i<10000;i++){
                assembled_reads[i] = -1;
        }
        i = 2000;
        while ((j >= 0)&&(i < 5000)){
                j = move_right_read(j, Reads, start_read);
                if (j != -1){
                        right_most_read = j;
                        assembled_reads[i] = j;
                        printf("Next Right Read %d(%d)\n",j, i);// getchar();
                        if (j == start_read){
                                j = -2;
                                while (assembled_reads[i1] != j){
                                        assembled_reads[i1] = -1;
                                        i1++;
                                }
                                if (assembled_reads[i1] == j)
                                        assembled_reads[i1] = -1;
                                j = -2;
                                printf("Circular Complex !\n");
                        }
                }
                i++;
        }
        if (j == -2){
                printf("Contig: Circular Successful! %d \n");
                generate_circular_contig(assembled_reads,  Reads, kmer, assembly_reads, assembly_genome);
        }else if (j== -1){
                printf("Contig: linear !!!\n");
                generate_circular_contig(assembled_reads,  Reads, kmer, assembly_reads, assembly_genome);
        }

}

int main (int argc, char* argv[])
{	
	int read_index, kmer_index;
	int* hashtable = (int *) malloc(1050000*sizeof(int));
	int reads_cov = atoi(argv[6]);
	int kmerlength = atoi(argv[7]);
        clock_t start, end;
        double cpu_time_used;
        struct kmer_state * kmer;
	struct read_state * Reads;

	//read all the reads from file argv[1] to Reads, and write the numbered reads to file argv[5], get the number of reads as read_index	
	start = clock();	
	read_Reads_file(argv[1], argv[5], &Reads, &read_index);
	end = clock();
	printf("Reading %d reads complete(%lf seconds)!\n", read_index, ((double) (end - start)) / CLOCKS_PER_SEC); //getchar();


	//read all the kmers from file argv[2] to kmer, get the number of kmers as kmer_index
        start = clock();
	read_kmer_file(argv[2], &kmer, hashtable, kmerlength, &kmer_index);
        end = clock();
        printf("Reading %d solid kmers(%lf seconds)!\n", kmer_index, ((double) (end - start)) / CLOCKS_PER_SEC); //getchar();
	
	//index all the reads for its kmers in kmer
	start = clock();
	indexing_Reads_kmer(Reads, kmer, hashtable, read_index, kmer_index, kmerlength);
        end = clock();
        printf("Indexing reads (%lf seconds)!\n", ((double) (end - start)) / CLOCKS_PER_SEC); //getchar();

        //assemble all the reads, and write the assembly in terms of reads to file argv[3], and in terms of preassembled-genome to file argv[4]
        start = clock();
	assemble_reads(Reads, kmer, read_index, reads_cov, argv[3], argv[4]);
        end = clock();
        printf("printing genome (%lf seconds)!\n", ((double) (end - start)) / CLOCKS_PER_SEC); //getchar();

	free(kmer);
	free(Reads);	

}
