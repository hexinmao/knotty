/***************************************************************************
                          common.cpp  -  description
                             -------------------
    begin                : Thu Apr 11 2002
    copyright            : (C) 2002 by Mirela Andronescu
    email                : andrones@cs.ubc.ca
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


// This file contains common functions, that may be used throughout the library

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "constants.h"
#include "structs.h"
#include "externs.h"
#include "common.h"


std::unique_ptr<asymmetry_penalties_class> asymmetry_penalties;

// Simply creates the class object with nb_nucleotides and assigns it to the pointer
void create_asymmetry_penalties(int nb_nucleotides) {
    asymmetry_penalties = std::unique_ptr<asymmetry_penalties_class>(new asymmetry_penalties_class(nb_nucleotides));
}

// pre-compute asymmetry penalties
// nb_nucleotides is the length of the sequence (number of nucleotides)
asymmetry_penalties_class::asymmetry_penalties_class(int nb_nucleotides)
{
    // possible inputs: 0 < size1 < MAXLOOP and 0 < size2 < nb_nucleotides-1
    // so vector is accessed by [0..MAXLOOP][0..nb_nucleotides-1]
    arr.resize(MAXLOOP+1);
    // for each possible size1 input:
    for(int size1 = 0; size1 < MAXLOOP+1; ++size1) {
        arr[size1].resize(nb_nucleotides);
        // for each possible size2 input:
        for (int size2 = 0; size2 < nb_nucleotides; ++size2) {
            // calculate the result at input (size1,size2) and store at arr[size1][size2]
            PARAMTYPE penalty = 0;
            if (parsi_asymmetry == T99)
                penalty = MIN (misc.asymmetry_penalty_max_correction, abs (size1-size2) * misc.asymmetry_penalty_array [MIN (2, MIN (size1, size2))-1]);
            //printf ("Asym penalty real: %d\n", penalty);
            else
            {
                if (size1 == size2) arr[size1][size2] = 0;
                if (parsi_asymmetry == PARSI)
                {
                    penalty = (PARAMTYPE) (internal_asymmetry_initiation + internal_asymmetry_slope * log (abs (size1-size2)));
                }
                else if (parsi_asymmetry == LAVISH)
                {
                    if (abs (size1-size2) < MAXLOOP_ASYM)
                    {
                        // we assume the following model: from asymmetry 1 to 4, we use initiation, slope and int_asym (like an addition)
                        if (abs (size1-size2) <= MAX_EXP_ASYM)
                        {
                            penalty += internal_asymmetry_initiation;
                            penalty += (PARAMTYPE) (log (abs (size1-size2)) * internal_asymmetry_slope);
                            penalty += internal_asymmetry[(int)(abs (size1-size2))];
                        }
                        else
                        {
                            penalty += internal_asymmetry[(int)(abs (size1-size2))];
                        }
                    }
                    else
                    {
                        penalty += internal_asymmetry_initiation;
                        penalty += (PARAMTYPE) (log (abs (size1-size2)) * internal_asymmetry_slope);
                    }
                }
            }
            arr[size1][size2] = penalty;
        }
    }

    // I tried the following for MODEL == EXTENDED, but I changed my mind
/*    // assume the size1 + size2 <= MAXLOOP_I. If it's greater, just use the value of the last parameter
    // first symmetric
    if (size1 == size2)
    {
        if (size1 <= MAXLOOP_I/2)        return  internal_symmetry[size1];
        return internal_symmetry[MAXLOOP_I/2];
    }
    // next asymmetric
    if (size1 + size2 <= MAXLOOP_I)      return internal_asymmetry [abs (size1-size2)];
    return internal_asymmetry[MAXLOOP_I-2];    */
}


void get_sorted_positions (int n, double numbers[], int positions[])
// used by suboptimal sorting
// does not modify numbers
{
    int i,j;
    double min;
    int sorted[MAXSUBSTR];
    for (i=0; i < n; i++) { sorted[i] = 0; }
    for (i=0; i < n; i++)
    {
        min = INF;
        for (j=0; j<n; j++)
        {
            if (!sorted[j])
            {
                if (numbers[j] < min)
                {
                    min = numbers[j];
                    positions[i] = j;
                }
            }
        }
        sorted[positions[i]] = 1;
    }
}




int is_structured (int i, int j, char *structure)
// return 1 if structure has some parentheses between i and j inclusive
// return 0 otherwise
{
    int k;
    for (k=i; k <= j; k++)
    {
        if (structure[k] != '.')
        {
            return 1;
        }
    }
    return 0;
}

int loss (int first, int last)
// known_pairings and pred_pairings are global variables
// known_pairings contains the pairings from 0 to n-1, of the reference structure
// pred_pairings contains the pairings of a potential structure on the region first-last inclusive
//      the other regions don't matter
// Returns the "Hamming" distance between known_pairings and pred_pairings on the region first-last inclusive
// This function is used for the loss-augmented prediction
// Written on August 9, 2008
// Note: Maybe this measure is better than the Hamming distance:
//      (# correctly predicted bp - # incorrectly predicted bp) / # true bp.
//      This will be in (-inf,1], but it only includes the base pairs, 
//      whereas the Hamming distance measure also includes the unpaired bases.
{
    if (known_pairings == NULL) return 0;
    if (first > last)
    {
        printf ("first %d should be <= last %d!!", first, last);
        exit (1);
    }
    int i;
    int distance = 0;
    for (i = first; i <= last; i++)
    {
        if (known_pairings[i] != pred_pairings[i])
            distance++;
    }
    return distance;
}


double compute_accuracy (char *ref_structure, char *pred_structure)
{
    int ptable_ref[MAXSLEN];
    int ptable_pred[MAXSLEN];
    int distance;
    int len, i;
    double accuracy;
    
    len = strlen(ref_structure);
    detect_original_pairs (ref_structure, ptable_ref);    
    detect_original_pairs (pred_structure, ptable_pred);
    distance = 0;
    for (i=0; i < len; i++)
    {
        if (ptable_pred[i] != ptable_ref[i])
            distance ++;
    }
    accuracy = 1.0-(double)distance/len; 
    return accuracy;
}

// Added on Sep 3, 2008, for loss-augmented prediction
double compute_distance (char *ref_structure, char *pred_structure)
// It has to be the same mathematical function as the one implemented in "loss"
// I'm leaving it double in case I'll want to change the function later with something more complicated
{
    int ptable_ref[MAXSLEN];
    int ptable_pred[MAXSLEN];
    int distance;
    int len, i;
    
    len = strlen(ref_structure);
    detect_original_pairs (ref_structure, ptable_ref);    
    detect_original_pairs (pred_structure, ptable_pred);
    distance = 0;
    for (i=0; i < len; i++)
    {
        if (ptable_pred[i] != ptable_ref[i])
            distance ++;
    }
    return (double) distance;
}


double compute_sensitivity (char *ref_structure, char *pred_structure)
// returns 0 if undefined (denominator is 0)
{
    int ptable_ref[MAXSLEN];
    int ptable_pred[MAXSLEN];
    int distance;
    int len, i;
    double sens;
    int num_correct_bp;
    int num_true_bp;
        
    len = strlen(ref_structure);
    detect_original_pairs (ref_structure, ptable_ref);    
    detect_original_pairs (pred_structure, ptable_pred);
    num_correct_bp = 0;
    num_true_bp = 0;
    for (i=0; i < len; i++)
    {
        if (ptable_ref[i] > -1)    // paired base
        {
            num_true_bp++;
            if (ptable_pred[i] == ptable_ref[i])
                num_correct_bp++;
        }                
    }
    if (num_true_bp == 0)
        return 0.0;
    sens = num_correct_bp*1.0/num_true_bp;
    return sens;
}


double compute_ppv (char *ref_structure, char *pred_structure)
// returns 0 if undefined (denominator is 0)
{
    int ptable_ref[MAXSLEN];
    int ptable_pred[MAXSLEN];
    int distance;
    int len, i;
    double ppv;
    int num_correct_bp;
    int num_pred_bp;
        
    len = strlen(ref_structure);
    detect_original_pairs (ref_structure, ptable_ref);    
    detect_original_pairs (pred_structure, ptable_pred);
    num_correct_bp = 0;
    num_pred_bp = 0;
    for (i=0; i < len; i++)
    {
        if (ptable_ref[i] > -1 && ptable_pred[i] == ptable_ref[i])    // paired base
            num_correct_bp++;
        if (ptable_pred[i] > -1)    // paired base
            num_pred_bp++;            
    }
    if (num_pred_bp == 0)
        return 0.0;
    ppv = num_correct_bp*1.0/num_pred_bp;
    return ppv;
}


double compute_pf_ppv (char *ref_structure, s_partition_function *part, double threshold)
// compute the positive predictive value obtained after thresholding the base pair probabilities
// part is the partition function object, which contains base pair probabilities
// returns -1 if undefined (denominator is 0)
{
    int ptable_ref[MAXSLEN];
    int distance;
    int len, i, j;
    double ppv=0.0;
    int num_correct_bp;
    int num_pred_bp;
        
    // prob begins from 1, and ptable_ref begins from 0

    len = strlen(ref_structure);
    detect_original_pairs (ref_structure, ptable_ref);    

    num_correct_bp = 0;
    num_pred_bp = 0;
    
    
    for (i=0; i < len; i++)
    {
        for (j=i+1; j < len; j++)
        {
            if (part->get_probability(i,j) >= threshold && ptable_ref[i] == j)
                num_correct_bp++;
            if (part->get_probability(i,j) >= threshold)
                num_pred_bp++;            
        }
    }

    /*
    for (i=0; i < len; i++)
    {
        if (ptable_ref[i] > i)
            printf ("\treal[%d]=%d\n", i, ptable_ref[i]);
        for (j=i+1; j < len; j++)
        {
            if (prob[i+1][j+1] >= threshold)
                printf ("\tprob[%d][%d]=%.2lf\n", i+1, j+1, prob[i+1][j+1]); 
        }
    }
    printf ("num_correct_bp=%d, num_pred_bp=%d\n", num_correct_bp, num_pred_bp);
    */
    
    if (num_pred_bp == 0)
    {
        //printf ("Undefined ppv\n");
        printf ("Thr=%.2lf, ppv=%.2lf, nu_corr=%d, num_pred=%d\n", threshold, ppv, num_correct_bp, num_pred_bp);
        return 0.0;
    }
    ppv = num_correct_bp*1.0/num_pred_bp;
    printf ("Thr=%.2lf, ppv=%.2lf, nu_corr=%d, num_pred=%d\n", threshold, ppv, num_correct_bp, num_pred_bp);        
    return ppv;
}


double compute_pf_sensitivity (char *ref_structure, s_partition_function *part, double threshold)
// compute the sensitivity obtained after thresholding the base pair probabilities
// part is the partition function object, which contains base pair probabilities
// returns -1 if undefined (denominator is 0)
{
    int ptable_ref[MAXSLEN];
    int distance;
    int len, i, j;
    double sens=0.0;
    int num_correct_bp;
    int num_true_bp;
        
    len = strlen(ref_structure);
    detect_original_pairs (ref_structure, ptable_ref);    
    num_correct_bp = 0;
    num_true_bp = 0;

    /*
    for (i=0; i < len; i++)
    {
        if (ptable_ref[i] > i)
            printf ("\treal[%d]=%d\n", i, ptable_ref[i]);
        for (j=i+1; j < len; j++)
        {
            if (prob[i+1][j+1] >= threshold)
                printf ("\tprob[%d][%d]=%.2lf\n", i+1, j+1, prob[i+1][j+1]); 
        }
    }
    */
    for (i=0; i < len; i++)
    {
        if (ptable_ref[i] > i)    // paired base
        {
            num_true_bp++;
            for (j=i+1; j < len; j++)
            {
                if (part->get_probability(i,j) >= threshold && ptable_ref[i] == j)
                    num_correct_bp++;
            }
        }                
    }
    if (num_true_bp == 0)
    {
        printf ("Undefined sens\n");
        return 0.0;
    }
    sens = num_correct_bp*1.0/num_true_bp;
    return sens;
}



void giveup (const char *string1, const char *string2)
// to add: variable nb of parameters, as in scanf, printf
{
    char temp[100];
    sprintf (temp, "%s %s", string1, string2);
    perror (temp);
    exit(1);
}


void create_random_sequence (int length, char *sequence)
// function to create uniformly random sequences - for demonstration purposes
{
    int rnumber, i;
    char base;
    for (i=0; i< length; i++)
    {
        rnumber = rand() % 100;
        if (rnumber >=0 && rnumber < 25)
            base = 'A';
        else if (rnumber >= 25 && rnumber < 50 )
            base = 'C';
        else if (rnumber >= 50 && rnumber < 75)
            base = 'G';
        else
            base = 'U';
        sequence[i] = base;
    }
    sequence[i] = '\0';
}

void create_random_restricted (char *sequence, char *restricted)
// sequence is an input argument
// restricted is the output argument
{
    int len = strlen (sequence);
    int i;
    int rnumber1, rnumber2;
    int rnumber = rand() % len;
    for (i=0; i < len; i++)
    {
        restricted[i] = '_';
    }
    restricted[i] = '\0';
    restricted[rnumber] = '.';
    // try a limited number of times
    for (i=0; i < 20; i++)
    {
        rnumber1 = rand() % len;
        rnumber2 = rand() % len;
        if (can_pair (nuc_to_int(sequence[rnumber1]), nuc_to_int(sequence[rnumber2])))
        {
            restricted [MIN (rnumber1, rnumber2)] = '(';
            restricted [MAX (rnumber1, rnumber2)] = ')';
            break;
        }
    }    
}


void remove_space (char *structure)
// PRE: none
// POST: remove the space(s) from structure, if any; modifies structure
{
    char str2[MAXSLEN];
    int len,i,j;
    len = strlen(structure);
    j = 0;
    for (i=0; i < len; i++)
    {
        if (structure[i] != ' ')
            str2[j++] = structure[i];
    }
    str2[j] = '\0';    
    strcpy (structure, str2);    
}


void empty_string (char * str)
// PRE:  str is a string
// POST: Put '\0' at all positions
{
    int i;
    int len;
    len = strlen(str);
    for (i=0; i< len; i++)
        str[i] = '\0';
}

int can_pair (int base1, int base2)
// PRE:  base1 and base2 are nucleotides over the alphabet {A, C, G, T, U}
// POST: return 1 if they can pair, 0 otherwise
{
    switch (base1)
    {
        case A:
            switch (base2)
            {
                case U: return 1;
                default: return 0;
            }
        case C:
            switch (base2)
            {
                case G: return 1;
                default: return 0;
            }                        
        case G:
            switch (base2)
            {
                case C: case U: return 1;
                default: return 0;
            }
        default:
            switch (base2)
            {
                case A: case G: return 1;
                default: return 0;
            }               
    }  
}



int watson_crick (int base1, int base2)
// PRE:  base1 and base2 are nucleotides over the alphabet {A, C, G, T, U}
// POST: return 1 if they are watson crick pair, 0 otherwise
{
    switch (base1)
    {
        case A:
            switch (base2)
            {
                case U: return 1;
                default: return 0;
            }
        case C:
            switch (base2)
            {
                case G: return 1;
                default: return 0;
            }                        
        case G:
            switch (base2)
            {
                case C: return 1;
                default: return 0;
            }
        default:
            switch (base2)
            {
                case A: return 1;
                default: return 0;
            }               
    }  
}


int nuc_to_int (char nucleotide)
// PRE:  nucleotide is 'A', 'C', 'G' or 'T'
// POST: Return 0 for A, 1 for C, 2 for G, 3 for T
{
    switch(nucleotide)
    {
        case 'a':
        case 'A': return A;
        // In the thermodynamic set, the experiments in Chen_Turner_2005 and Chen_Turner_2006b contain P. For the MODEL = SIMPLE, we consider this is an A.
        case 'p':
        case 'P': return A;
        case 'c':
        case 'C': return C;
        case 'g':
        case 'G': return G;
        default : return U;
    }
}


/*
// by Cho MinSik
 int nuc_to_int (char nucleotide)
// PRE:  nucleotide is 'A', 'C', 'G' or 'T'
// POST: Return 0 for A, 1 for C, 2 for G, 3 for T

{
    static bool bInitialized    =   false;
    static int  iNucleotide['z'];

    if(!bInitialized)
    {
        bInitialized    =   true;
        iNucleotide['a']    =   A;
        iNucleotide['A']    =   A;
        iNucleotide['c']    =   C;
        iNucleotide['C']    =   C;
        iNucleotide['g']    =   G;
        iNucleotide['G']    =   G;
        iNucleotide['t']    =   U;
        iNucleotide['T']    =   U;
        iNucleotide['u']    =   U;
        iNucleotide['U']    =   U;
    }
    return iNucleotide[nucleotide];
}
*/

char int_to_nuc (int inuc)
{
    switch(inuc)
    {
        case A: return 'A';
        case C: return 'C';
        case G: return 'G';
        default: return 'U';
    }
}


int is_nucleotide (char base)
// PRE:  base is a character 
// POST: return true if base is a nucleotide (A, C, G or T)
//     return false otherwise
{
    return (base == 'A' || base == 'C' || base == 'G' || base == 'T' || base == 'U' ||
            base == 'a' || base == 'c' || base == 'g' || base == 't' || base == 'u');
}


void check_sequence (char *sequence)
// check sequence for length and alphabet
{
    int length;
    int maxlen;
    int i;
    length = strlen (sequence);
    maxlen = MAXSLEN;
    if (length == 0)
    {
        printf ("Empty sequence\n");
        exit (1);
    }
    if (length > MAXSLEN)
    {
        printf ("Sequence too long; maximum allowed: %d\n", maxlen);
        exit (1);
    }
    for (i=0; i<length; i++)
    {
        if (!is_nucleotide (sequence[i]))
        {
            printf ("Sequence not valid: %c found\n", sequence[i]);
            printf ("Only acgtuACGTU are allowed\n");
            exit (1);
        }
    }
}


/*
PARAMTYPE IL_penalty_by_size_2D (int size1, int size2)
// for internal loops
{
    #if (MODEL == EXTENDED)
    if (size1 + size2 <= MAXLOOP_I)     return internal_penalty_by_size_2D[size1][size2];
    // if it's larger, apply the large length formula to the last penalty with the same relative asymmetry
    int i = MIN (size1, size2);
    int j = MAX (size1, size2);
    // find x and y, where x <=y such that i/j=x/y and x+y=MAXLOOP_I
    int x, y;
    y = MAXLOOP_I*j/(i+j);
    x = MAXLOOP_I - y;
    double logval;
    logval = log (1.0*(i+j)/(x+y));
    return internal_penalty_by_size_2D[x][y] + (PARAMTYPE)(100.0*misc.param_greater30 * logval);
    #endif
}
*/

std::unique_ptr<size_penalties_class> size_penalties;

// Simply creates the class object with nb_nucleotides and assigns it to the pointer
void create_size_penalties(int nb_nucleotides) {
    size_penalties = std::unique_ptr<size_penalties_class>(new size_penalties_class(nb_nucleotides));
}

// precompute size penalties
// nb_nucleotides is the length of the sequence (number of nucleotides)
size_penalties_class::size_penalties_class(int nb_nucleotides) {
    // for each possible output, calculate output and store in arr[type][size]
    // possible inputs: 0 < size < nb_nucleotides-1 and type = H(0), B(1), or I(2)
    // so vector is accessed by [0..2][0..nb_nucleotides-1]
    arr.resize(3);
    // for each type (0..2)
    for(int type = 0; type < 3; type++) {
        // 0 is H, 1 is B, 2 is I
        int end;
        if (parsi_length == T99)
        {
            switch(type) {
                case 0:
                    end = MAXLOOP_H_T99; break;
                case 1:
                    end = MAXLOOP_B_T99; break;
                case 2:
                    end = MAXLOOP_I_T99; break;
                default:
                    printf("Error in simfold/src/common/common.cpp creation of size_penalties\n");
                    exit(-1);
            }
        }
        else if (parsi_length == PARSI || parsi_length == ZL)
        {
            switch(type) {
                case 0:
                    end = MAXLOOP_H_PARSI; break;
                case 1:
                    end = MAXLOOP_B_PARSI; break;
                case 2:
                    end = MAXLOOP_I_PARSI; break;
                default:
                    printf("Error in simfold/src/common/common.cpp creation of size_penalties\n");
                    exit(-1);
            }
        }
        else if (parsi_length == LAVISH)
        {
            switch(type) {
                case 0:
                    end = MAXLOOP_H_LAVISH; break;
                case 1:
                    end = MAXLOOP_B_LAVISH; break;
                case 2:
                    end = MAXLOOP_I_LAVISH; break;
                default:
                    printf("Error in simfold/src/common/common.cpp creation of size_penalties\n");
                    exit(-1);
            }
        }

        arr[type].resize(nb_nucleotides);
        // for each possible size (0..nb_nucleotides-1)
        for (int size=0; size < nb_nucleotides; size++) {
            PARAMTYPE penalty30, penalty;
            double logval;

            // the penalties for size <= MAXLOOP _H, _B, _I should be read from the file "loop"
            //if (size <= MAXLOOP)
            if (type == 0 && size <= end)
            {
                penalty = hairpin_penalty_by_size[size];
            }
            else if (type == 1 && size <= end)
            {
                penalty = bulge_penalty_by_size[size];
            }
            else if (type == 2 && size <= end)
            {
                penalty = internal_penalty_by_size[size];
            }
            else {
                //return 50.0;
                // size > MAXLOOP _H, _B, _I
                if (type == 0)
                {
                    penalty30 = hairpin_penalty_by_size[end];
                    logval = log (1.0*size/end);
                }
                else if (type == 1)
                {
                    penalty30 = bulge_penalty_by_size[end];
                    logval = log (1.0*size/end);
                }
                else if (type == 2)
                {
                    penalty30 = internal_penalty_by_size[end];
                    logval = log (1.0*size/end);
                }
                else
                {
                    printf ("ERROR! type is not valid, ABORT!\n");
                    exit(1);
                }

                penalty = (PARAMTYPE) (penalty30 + 100.0*misc.param_greater30 * logval);
            }

            arr[type][size] = penalty;
        }
    }
}

PARAMTYPE penalty_by_size_enthalpy (int size, char type)
// PRE:  size is the size of the loop
//       type is HAIRP or INTER or BULGE
// POST: return the penalty by size of the loop
{

    // TODO: if I want to use this for parameter learning, I have to replace MAXLOOP by _B, _I, _H.
    PARAMTYPE penalty30, penalty;

    // the penalties for size <= MAXLOOP should be read from the file "loop"
    if (size <= MAXLOOP)
    {
        if (type == 'H')
            return enthalpy_hairpin_penalty_by_size[size];
        if (type == 'I')
            return enthalpy_internal_penalty_by_size[size];
        return enthalpy_bulge_penalty_by_size[size];
    }

    // size > MAXLOOP
    if (type == 'H')
        penalty30 = enthalpy_hairpin_penalty_by_size[MAXLOOP];
    else if (type == 'I')
        penalty30 = enthalpy_internal_penalty_by_size[MAXLOOP];
    else
        penalty30 = enthalpy_bulge_penalty_by_size[MAXLOOP];

    penalty = (int) (penalty30 + round(enthalpy_misc.param_greater30 * log(((double)size)/30)));

    return penalty;
}


void substr (char *source, int begin, int end, char *dest)
// PRE:  begin and end are smaller than strlen(source)
// POST: Put in dest what is in source between position begin and position end
{
    int i;
    for (i = 0; i <= end - begin; i++)
        dest[i] = source[i+begin];
    dest[i] = '\0';
}


void replace_str_piece (char *sequence, int position, char *seq)
// PRE:  begin + strlen(seq) < strlen (sequence)
// POST: In sequence, at position, replace what is was by seq
{
    int i;
    for (i = 0; i < strlen(seq); i++)
        sequence[position+i] = seq[i];
}


char complement (char base)
// PRE:  Base is an RNA nucleotide
// POST: Return the complement of base
{
  switch (base)
  {
    case 'a': case 'A': return 'U';
    case 'c': case 'C': return 'G';
    case 'g': case 'G': return 'C';
    default: return 'A';
  }
}


void reverse_complement_of_seq (const char *seq, char *complem)
// PRE:  seq is a sequence
// POST: complement and reverse sequence and put the result into compl
{
    int seq_len, i;
    seq_len = strlen(seq);
    for (i=0; i< seq_len; i++)
    {
        complem[i] = complement (seq[seq_len-i-1]);
    }
    complem[seq_len] = '\0';
}




void insert_space (char *structure, int place)
// PRE:  None
// POST: insert a space at the specified place, in structure
{
    int i;
    int length;
    char str[MAXSLEN];
    length = strlen (structure);
    for (i=0; i <= place; i++)
        str[i] = structure[i];
    str[i] = ' ';
    for (i=place+1; i < length; i++)
        str[i+1] = structure[i];
    str[i+1]= '\0';
    //strncpy (structure, str, length+1);
    for (i = 0; i <= length; i++)
        structure[i] = str[i];
    structure[i] = '\0';    
    //strcpy (structure, str);
}


void init (stack_ds *st)
// PRE:  None
// POST: Initialize the stack st
{
        st->top = 0;
}

void push (stack_ds *st, int el)
// PRE:  st is a valid stack
// POST: Push an element to the stack
{
        st->elem[st->top++] = el;
}

int pop (stack_ds *st)
// PRE:  st is a valid stack, that is not empty
// POST: pop an element from the stack and return it
{
    if (st->top <= 0)
    {
        fprintf (stderr, "The given structure is not valid: more right parentheses than left parentheses\n");
        exit (1);
    }
        return st->elem[--st->top];
}



void detect_original_pairs(char *structure, int *p_table)
// PRE:  structure contains the desired structure
// POST: p_table will contain the index of each base pair
//               or -1 if it does not pair
// Feb 28, 2008: structure can also have:
//  - angles: < or >, which denote the ends of a pseudoknot. In that case, p_table would still be filled in the same way.
//      The assumption is that the <> pairs are always nested within parentheses.
//      That is, a structure like this (<)> is not possible.
//  - x, which denotes that I should ignore that part. p_table would be -3 in that case/
{
        int i, j, struct_len;
        stack_ds st;
        init (&st);
        remove_space (structure);
        struct_len = strlen (structure);
        for (i=0; i < struct_len; i++)
          {
            if (structure[i] == '.') 
              p_table[i] = -1;
            else if (structure[i] == ' ' || structure[i] == '_')
              p_table[i] = -2;
            else if (structure[i] == 'x')
              p_table[i] = -3;
            else if (structure[i] == '(' || structure[i] == '<')
              push (&st, i);
            else if (structure[i] == ')' || structure[i] == '>')
              {
                j = pop (&st);
                p_table[i] = j;
                p_table[j] = i;
              }
          }
        if (st.top != 0)
        {
            fprintf (stderr, "The given structure is not valid: %d more left parentheses than right parentheses: %s\n", st.top, structure);
            exit (1);
        }
}


int valid_structure (int i, int j, char *structure)
// returns 1 if this structure is valid (i.e. complete), 0 if it's partial
{
    int k;
    stack_ds st;
    init (&st);
    for (k=i; k <= j; k++)
    {
        if (structure[k] == '(')
            push (&st, k);
        else if (structure[k] == ')')
        {
            if (st.top == 0)
                return 0;    // more right parentheses
            pop (&st);
        }
    }
    if (st.top != 0)
        return 0;    // more left parentheses
    return 1;
}


void detect_structure_features (char *structure, str_features *f)
// PRE:  None
// POST: The variable f is filled with structure features, i.e. what type of elementary structure
//       this base is closing (such as stacked pair, hairpin loop etc.)
// Modified on Feb 28, 2008
//  Added the case when structure can also have angles and x's.
{
    int num_branches, i, j;
    int p_table[MAXSLEN];
    int bri[MAX_BRANCHES];
    int nb_nucleotides;

    nb_nucleotides = strlen(structure);
    detect_original_pairs (structure, p_table);
    for (i=0; i < nb_nucleotides; i++)
    {
        f[i].pair = p_table[i];
        if (p_table[i] > i)
        {
            f[p_table[i]].pair = i;
            // this base pair might be angle brackets or parentheses.            
            if (structure[i] == '<')
            {
                // first make sure the pair is also angle bracket
                if (structure[p_table[i]] != '>')
                {
                    printf ("ERROR! structure is not valid, position %d should be > and is %c\n%s\n", p_table[i], structure[p_table[i]], structure);
                    exit(1);
                }
                f[i].type = NONE;
                f[p_table[i]].type = NONE;
                // whatever is inside the angle brackets should be x's. Make sure that's true.
                // I don't need to make this a strong restriction
//                 for (j=i+1; j < p_table[i]; j++)
//                 {
//                     if (structure[j] != 'x')
//                     {
//                         printf ("ERROR! structure is not valid, position %d should be x and is %c\n%s\n", j, structure[j], structure);
//                         exit(1);
//                     }
//                 }
                continue;                
            }
            // if we got here, it means the base pair was ()
            // just make sure the partner is )
            if (structure[p_table[i]] != ')')
            {
                printf ("ERROR! structure is not valid, position %d should be ) and is %c\n%s\n", p_table[i], structure[p_table[i]], structure);
                exit(1);
            }

            // check if it is stacked pair
            if (p_table[i+1] == p_table[i]-1 && p_table[i+1] > i+1)
            {
                // if the next pair is an angle pair, then this is a multi-loop, deal with it below
                if (structure[i+1] != '<')
                {
                    f[i].type = STACK;
                    f[p_table[i]].type = STACK;
                    continue;
                }
            }
            // check if it is hairpin, internal loop or multi-loop
            num_branches = 0;
            // one of the branches can be <xxx> and this is a multi-loop no matter what
            int is_multi_loop = 0;
            for (j=i+1; j < p_table[i]; j++)
            {
                if (p_table[j] > j)
                {
                    if (structure[j] == '<')
                        is_multi_loop = 1;
                    bri[num_branches] = j;
                    num_branches++;
                    j = p_table[j];
                }
            }
            if (num_branches == 0)  // hairpin
            {
                int ignore_hairpin = 0;
                // check whether this hairpin loop should be ignored
                for (j=i+1; j < p_table[i]; j++)
                {
                    if (structure[j] == 'x')
                    {
                        ignore_hairpin = 1;
                        break;
                    }
                }
                if (ignore_hairpin)
                {
                    f[i].type = NONE;
                    f[p_table[i]].type = NONE;
                }
                else
                {
                    f[i].type = HAIRP;
                    f[p_table[i]].type = HAIRP;
                }
            }
            else if (num_branches == 1 && !is_multi_loop) // internal loop
            {
                // TODO: test if we have x's inside
                f[i].type = INTER;
                f[p_table[i]].type = INTER;
                f[i].num_branches = 1;
                f[i].bri[0] = bri[0];
            }
            else    // multi loop
            {
                // TODO: test if we have x's inside
                f[i].type = MULTI;
                f[p_table[i]].type = MULTI;
                f[i].num_branches = num_branches;
                for (j=0; j < num_branches; j++)
                    f[i].bri[j] = bri[j];
            }
        }
    }
    /*
    for (i=0; i < nb_nucleotides; i++)
    {
        if (f[i].pair > i)
        {
            printf ("%d - pair: %d type: %c ", i, f[i].pair, f[i].type);
            if (f[i].type == INTER)
                printf ("(%d,%d)", f[i].bri[0], f[f[i].bri[0]].pair);
            else if (f[i].type == MULTI)
                for (j=0; j < f[i].num_branches; j++)
                    printf ("(%d,%d) ", f[i].bri[j], f[f[i].bri[j]].pair);
            printf ("\n");
        }
    }
    */
}


int complementary_bases (char b1, char b2)
// returns 1 if b1 and b2 are complementary bases
{
    int base1, base2;
    base1 = nuc_to_int (b1);
    base2 = nuc_to_int (b2);
    switch (base1)
    {
        case A:
            switch (base2)
            {
                case U: return 1;
                default: return 0;
            }
        case C:
            switch (base2)
            {
                case G: return 1;
                default: return 0;
            }                        
        case G:
            switch (base2)
            {
                case C: return 1;
                default: return 0;
            }
        default:
            switch (base2)
            {
                case A: return 1;
                default: return 0;
            }               
    }      
}

int self_complementary (char *sequence)
// return 1 if this sequence is self-complementary
// self_complementary means the first half is the reverse complement of the second half
// if length (sequence) is an odd number, return 0
// Or should be: "the middle base does not matter"?

{
    int i, len;
    len = strlen (sequence);
    // if off, return 0;
    if ((len/2)*2 != len)
        return 0;
    for (i=0; i < len/2; i++)
    {
        if (!complementary_bases (sequence[i], sequence[len-i-1]))
            return 0;
    }
    return 1;
}


int exists_restricted (int i, int j, str_features *fres)
{
    int k;
    if (fres == NULL)
        return 0;
    for (k = i+1; k < j; k++)
    {
        if (fres[k].pair > -1)
            return 1;
    }
    return 0;
}

int exists_restricted_ptable (int i, int j, int *ptable)
{
    int k;
    if (ptable == NULL)
        return 0;
    for (k = i+1; k < j; k++)
    {
        if (ptable[k] > -1)
            return 1;
    }
    return 0;
}


void read_parsi_options_from_file (char *filename)
// the file should contain values (0 or 1) for each of the parsi options. For example the following is all-parsimonious options
//     parsi_tstackh = 1;
//     parsi_tstacki = 1;
//     parsi_asymmetry = 1;
//     parsi_int11 = 1;
//     parsi_int21 = 1;
//     parsi_int22 = 1;
//     parsi_bulge1 = 1;
//     parsi_dangles = 1;
//     parsi_others = 1;
//     parsi_length = 1;
//     parsi_special = 1;
{
    FILE *file;
    char buffer[100];
    int value;
    char option[100];
    if ((file = fopen (filename, "r")) == NULL)
    {
        giveup ("Cannot open file", filename);
    }

    fgets (buffer, sizeof(buffer), file);
    while (!feof (file))
    {
        //printf ("buffer = %s\n", buffer);                
        sscanf (buffer, "%s = %d", option, &value);
        //printf ("option = |%s|, value = |%d|\n", option, value);
        if (strcmp (option, "parsi_tstackh") == 0)          parsi_tstackh = value;
        else if (strcmp (option, "parsi_tstacki") == 0)     parsi_tstacki = value;
        else if (strcmp (option, "parsi_asymmetry") == 0)   parsi_asymmetry = value;
        else if (strcmp (option, "parsi_int11") == 0)       parsi_int11 = value;
        else if (strcmp (option, "parsi_int21") == 0)       parsi_int21 = value;
        else if (strcmp (option, "parsi_int22") == 0)       parsi_int22 = value;
        else if (strcmp (option, "parsi_bulge1") == 0)      parsi_bulge1 = value;
        else if (strcmp (option, "parsi_dangles") == 0)     parsi_dangles = value;
        //else if (strcmp (option, "parsi_others") == 0)      parsi_others = value;
        else if (strcmp (option, "parsi_length") == 0)      parsi_length = value;
        else if (strcmp (option, "parsi_special") == 0)     parsi_special = value;
        else if (strcmp (option, "use_similarity_rules") == 0)     use_similarity_rules = value;
        fgets (buffer, sizeof(buffer), file);
    }    
    fclose (file);
//     printf ( "parsi_tstackh = %d\n", parsi_tstackh);
//     printf ( "parsi_tstacki = %d\n", parsi_tstacki);
//     printf ( "parsi_asymmetry = %d\n", parsi_asymmetry);
//     printf ( "parsi_int11 = %d\n", parsi_int11);
//     printf ( "parsi_int21 = %d\n", parsi_int21);
//     printf ( "parsi_int22 = %d\n", parsi_int22);
//     printf ( "parsi_bulge1 = %d\n",parsi_bulge1);
//     printf ( "parsi_dangles = %d\n", parsi_dangles);
//     printf ( "parsi_others = %d\n", parsi_others);
//     printf ( "parsi_length = %d\n", parsi_length);
//     printf ( "parsi_special = %d\n", parsi_special);
//     printf ( "use_similarity_rules = %d\n", use_similarity_rules);

    if (parsi_dangles == PARSI)     // no dangling ends
        no_dangling_ends = 1;
    else
        no_dangling_ends = 0;
                                  
}


