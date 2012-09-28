/*
 *  Aligner.cpp is part of the CRisprASSembler project
 *  
 *  Created by Connor Skennerton.
 *  Copyright 2011, 2012 Connor Skennerton & Michael Imelfort. All rights reserved. 
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 *                     A B R A K A D A B R A
 *                      A B R A K A D A B R
 *                       A B R A K A D A B
 *                        A B R A K A D A       	
 *                         A B R A K A D
 *                          A B R A K A
 *                           A B R A K
 *                            A B R A
 *                             A B R
 *                              A B
 *                               A
 */
#include <iostream>
#include <libcrispr/Exception.h>
#include "Aligner.h"
#include "LoggerSimp.h"
#include "SeqUtils.h"

void Aligner::setMasterDR(std::string& master) {
    AL_masterDRToken = mStringCheck->getToken(master);
    AL_Offsets[AL_masterDRToken] = (int)(AL_length * CRASS_DEF_CONS_ARRAY_START);
    prepareMasterForAlignment(master);
    placeReadsInCoverageArray(AL_masterDRToken);
    calculateDRZone();
    
}

void Aligner::alignSlave(StringToken& slaveDRToken) {
    
    AL_Offsets[slaveDRToken] = -1;
    std::string slaveDR = mStringCheck->getString(slaveDRToken);
    AlignerFlag_t flags;
    int offset = getOffsetAgainstMaster(slaveDR, flags);
    
    if (flags[score_equal]) {
        std::string extended_slave_dr;
        extendSlaveDR(slaveDR, extended_slave_dr);
        flags.reset();
        offset = getOffsetAgainstMaster(extended_slave_dr, flags);
        if (flags[score_equal]) {
            logWarn("@Alignment Warning: Extended Slave scores equal",4);
            logWarn("Cannot place slave: "<<slaveDR<<" ("<<slaveDRToken<<") in array", 4);
            logWarn("Original slave: "<<slaveDR, 4);
            logWarn("Extended Slave: "<<extended_slave_dr, 4);
            logWarn("Master: "<<mStringCheck->getString(AL_masterDRToken), 4);
            logWarn("******", 4);
            flags[failed] = true;
        }
    }
    
    if (flags[failed]) {
        return;
    } 
    
    if (flags[reversed] ) {
        // we need to reverse all the reads and the DR for these reads
        try {
            ReadListIterator read_iter = mReads->at(slaveDRToken)->begin();
            while (read_iter != mReads->at(slaveDRToken)->end()) 
            {
                (*read_iter)->reverseComplementSeq();
                read_iter++;
            }
        } catch(crispr::exception& e) {
            std::cerr<<e.what()<<std::endl;
            throw crispr::exception(__FILE__,
                                    __LINE__,
                                    __PRETTY_FUNCTION__,
                                    "cannot reverse complement sequence");
        }
        // fix the places where the DR is stored
        
        slaveDR = reverseComplement(slaveDR);
        StringToken st = mStringCheck->addString(slaveDR);
        mReads->at(st) = mReads->at(slaveDRToken);
        mReads->at(slaveDRToken) = NULL;
        slaveDRToken = st;
        //AL_Offsets[slaveDRToken] = -1;       
    }
    AL_Offsets[slaveDRToken] = AL_Offsets[AL_masterDRToken] + offset;
    
    placeReadsInCoverageArray(slaveDRToken);
}

void Aligner::generateConsensus() {
    
    logInfo("Calculating consensus sequence from aligned reads", 1)
#ifdef DEBUG
    logInfo("DR zone: " << AL_ZoneStart << " -> " << AL_ZoneEnd, 1);
#endif

	// chars we luv!
    char alphabet[4] = {'A', 'C', 'G', 'T'};

	
	// populate the conservation array
    int num_GT_zero = 0;
    for(int j = 0; j < AL_length; j++)
	{
		int max_count = 0;
		float total_count = 0;
		for(int i = 1; i <= 4; i++)
		{
			
			total_count += static_cast<float>(AL_coverage[i*j]);
			if(AL_coverage[i*j] > max_count)
			{
				max_count = AL_coverage[i*j];
				AL_consensus[j] = alphabet[i-1];
			}
		}
        
		// we need at least CRASS_DEF_MIN_READ_DEPTH reads to call a DR
		if(total_count > CRASS_DEF_MIN_READ_DEPTH)
		{
			AL_conservation[j] = static_cast<float>(max_count)/total_count;
			num_GT_zero++;
		}
		else
		{
			AL_conservation[j] = 0;
		}
	}
    
    // trim these back a bit (if we trim too much we'll get it back right now anywho)
    // CTS: Not quite true, if it is low coverage then there will be no extension!
    // check to see that this DR is supported by a bare minimum of reads
    if(num_GT_zero < CRASS_DEF_MIN_READ_DEPTH) {
        logWarn("**WARNING: low confidence DR", 1);
    } else {
        // first work from the left and trim back
	    while(AL_ZoneStart > 0)
	    {
		    if(AL_conservation[AL_ZoneStart - 1] < CRASS_DEF_ZONE_EXT_CONS_CUT_OFF) 
                AL_ZoneStart++;
            else 
			    break;
	    }
        
	    // next work from the right
	    while(AL_ZoneEnd < AL_length - 1)
	    {
		    if(AL_conservation[AL_ZoneEnd + 1] < CRASS_DEF_ZONE_EXT_CONS_CUT_OFF)
			    AL_ZoneEnd--;
		    else
			    break;
	    }
    }
	//same as the loops above but this time extend outward
	while(AL_ZoneStart > 0)
	{
		if(AL_conservation[AL_ZoneStart - 1] >= CRASS_DEF_ZONE_EXT_CONS_CUT_OFF) 
            AL_ZoneStart--;
        else 
			break;    
	}
    
	// next work to the right
	while(AL_ZoneEnd < AL_length - 1)
	{
		if(AL_conservation[AL_ZoneEnd + 1] >= CRASS_DEF_ZONE_EXT_CONS_CUT_OFF)
			AL_ZoneEnd++;
		else
			break;
	}
    
#ifdef DEBUG
    logInfo("DR zone (post fix): " << AL_ZoneStart << " -> " << AL_ZoneEnd, 1);
#endif

}

void Aligner::prepareSequenceForAlignment(std::string& sequence, uint8_t *transformedSequence) {

    size_t seq_length = sequence.length();
    size_t i;
    for (i = 0; i < seq_length; ++i) 
        transformedSequence[i] = seq_nt4_table[(int)sequence[i]];
    
    // null terminate the sequences
    transformedSequence[seq_length] = '\0';

}

void Aligner::prepareSlaveForAlignment(std::string& slaveDR, 
                                       uint8_t *slaveTransformedForward, 
                                       uint8_t *slaveTransformedReverse) {
    
    prepareSequenceForAlignment(slaveDR, slaveTransformedForward);
    std::string revcomp_slave_dr = reverseComplement(slaveDR);
    prepareSequenceForAlignment(revcomp_slave_dr, slaveTransformedReverse);
}

int Aligner::getOffsetAgainstMaster(std::string& slaveDR, AlignerFlag_t& flags) {

    int slave_dr_length = static_cast<int>(slaveDR.length());
    uint8_t slave_dr_forward[slave_dr_length+1];
    uint8_t slave_dr_reverse[slave_dr_length+1];
    
    prepareSlaveForAlignment(slaveDR, slave_dr_forward, slave_dr_reverse);

    // query profile 
    kswq_t *slave_forward_query_profile = 0;
    kswq_t *slave_reverse_query_profile = 0;
    
    
    
    // alignment of slave against master
    kswr_t forward_return = ksw_align(slave_dr_length, 
                                      slave_dr_forward, 
                                      AL_masterDRLength, 
                                      AL_masterDR, 
                                      5, 
                                      AL_scoringMatrix, 
                                      AL_gapOpening, 
                                      AL_gapExtension, 
                                      AL_xtra, 
                                      &slave_forward_query_profile);
    
    
    kswr_t reverse_return = ksw_align(slave_dr_length, 
                                      slave_dr_reverse, 
                                      AL_masterDRLength, 
                                      AL_masterDR, 
                                      5, 
                                      AL_scoringMatrix, 
                                      AL_gapOpening, 
                                      AL_gapExtension, 
                                      AL_xtra, 
                                      &slave_reverse_query_profile);
    
    
    // free the query profile
    free(slave_forward_query_profile); 
    free(slave_reverse_query_profile);
    
    // figure out which alignment was better
    if (reverse_return.score == forward_return.score) {
        flags[score_equal] = true;
        return 0;
    }
        
    if (reverse_return.score > forward_return.score && reverse_return.score >= AL_minAlignmentScore) {
        flags[reversed] = true;
        //std::cout<<"R: "<< masterDR<<"\t"<< reverse_return.tb<<"\t"<< reverse_return.te+1<<"\t"<< rev_slave<<"\t"<< reverse_return.qb<<"\t"<< reverse_return.qe+1<<"\t"<< reverse_return.score<<"\t"<< reverse_return.score2<<"\t"<< reverse_return.te2<<"\t"<<reverse_return.tb - reverse_return.qb<<std::endl;
        return reverse_return.tb - reverse_return.qb;
    } else if (forward_return.score >= AL_minAlignmentScore) {
        //std::cout<<"F: "<< masterDR<<"\t"<< forward_return.tb<<"\t"<< forward_return.te+1<<"\t"<< slaveDR<<"\t"<< forward_return.qb<<"\t"<< forward_return.qe+1<<"\t"<< forward_return.score<<"\t"<< forward_return.score2<<"\t"<< forward_return.te2<<"\t"<<forward_return.tb - forward_return.qb<<std::endl;
        return forward_return.tb - forward_return.qb;
    } else {
        logWarn("@Alignment Warning: Slave Score Failure",4);
        logWarn("Cannot place slave: "<<slaveDR<<" ("<<mStringCheck->getToken(slaveDR)<<") in array", 4);
        logWarn("Master: "<<AL_masterDR, 4);
        logWarn("Forward score: "<<forward_return.score, 4);
        logWarn("Reverse score: "<<reverse_return.score, 4);
        logWarn("******", 4);
        flags[failed] = true;
        return 0;
    }
    
    return 0;
}

void Aligner::placeReadsInCoverageArray(StringToken& currentDrToken) {
    // we need to correct for the fact that we may not be using the 0th kmer
    int positional_offset = AL_Offsets[currentDrToken];//(kmer_positions_DR_master)[0] - (kmer_positions_DR_master)[positioning_kmer_index] + (kmer_positions_ARRAY)[positioning_kmer_index];
    ReadListIterator read_iter = mReads->at(currentDrToken)->begin();
    int current_dr_length = static_cast<int>(mStringCheck->getString(currentDrToken).length());
    
    while (read_iter != mReads->at(currentDrToken)->end()) 
    {
        // don't care about partials
        int dr_start_index = 0;
        int dr_end_index = 1;
        while(((*read_iter)->startStopsAt(dr_end_index) - (*read_iter)->startStopsAt(dr_start_index)) != (current_dr_length - 1))
        {
            dr_start_index += 2;
            dr_end_index += 2;
        } 
        // go through every full length DR in the read and place in the array
        do
        {
            if(((*read_iter)->startStopsAt(dr_end_index) - (*read_iter)->startStopsAt(dr_start_index)) == (current_dr_length - 1))
            {
                // we need to find the first kmer which matches the mode.
                int this_read_start_pos = positional_offset - (*read_iter)->startStopsAt(dr_start_index);
                for(int i = 0; i < (int)(*read_iter)->getSeqLength(); i++)
                {
                    int index = -1;
                    switch((*read_iter)->getSeqCharAt(i))
                    {
                        case 'A':
                            index = 1;
                            break;
                        case 'C':
                            index = 2;
                            break;
                        case 'G':
                            index = 3;
                            break;
                        default:
                            index = 4;
                            break;
                    }
                    int index_b = i+this_read_start_pos; 
                    if((index_b) >= AL_length)
                    {
                        logError("***FATAL*** MEMORY CORRUPTION: The consensus/coverage arrays are too short");
                    }
                    if((index_b) < 0)
                    {
                        logError("***FATAL*** MEMORY CORRUPTION: index = "<< index_b<<" less than array begining");
                    }
                    
                    AL_coverage[index * index_b]++;
                }
            }
            // go onto the next DR
            dr_start_index += 2;
            dr_end_index += 2;
            
            // check that this makes sense
            if(dr_start_index >= (int)((*read_iter)->numRepeats()*2)) {
                break;
            }
            
        } while(((*read_iter)->startStopsAt(dr_end_index) - (*read_iter)->startStopsAt(dr_start_index)) == (current_dr_length - 1));
        read_iter++;
    }

}


void Aligner::extendSlaveDR(std::string &slaveDR, std::string &extendedSlaveDR){
 
    StringToken token = mStringCheck->getToken(slaveDR);
    
    // go into the reads and get the sequence of the DR plus a few bases on either side
    ReadListIterator read_iter = mReads->at(token)->begin();
    while (read_iter != mReads->at(token)->end()) 
    {
        // don't care about partials
        int dr_start_index = 0;
        int dr_end_index = 1;
        
        // Find the DR which is the right DR length.
        // compensates for partial repeats
        while(((*read_iter)->startStopsAt(dr_end_index) - (*read_iter)->startStopsAt(dr_start_index)) != ((int)(slaveDR.length()) - 1))
        {
            dr_start_index += 2;
            dr_end_index += 2;
        }
        // check that the DR does not lie too close to the end of the read so that we can extend
        if((*read_iter)->startStopsAt(dr_start_index) - 2 < 0 || (*read_iter)->startStopsAt(dr_end_index) + 2 > (*read_iter)->getSeqLength()) {
            // go to the next read
            read_iter++;
            continue;
        } else {
            // substring the read to get the new length
            extendedSlaveDR = (*read_iter)->getSeq().substr((*read_iter)->startStopsAt(dr_start_index) - 2, slaveDR.length() + 4);
        }
    }
}



void Aligner::calculateDRZone() {
    ReadListIterator read_iter = mReads->at(AL_masterDRToken)->begin();
    while (read_iter != mReads->at(AL_masterDRToken)->end()) 
    {
        // don't care about partials
        int dr_start_index = 0;
        int dr_end_index = 1;
        
        // Find the DR which is the master DR length.
        // compensates for partial repeats
        while(((*read_iter)->startStopsAt(dr_end_index) - (*read_iter)->startStopsAt(dr_start_index)) != (AL_masterDRLength - 1))
        {
            dr_start_index += 2;
            dr_end_index += 2;
        }
        
        //  This if is to catch some weird-ass scenario, if you get a report that everything is wrong, then you've most likely
        // corrupted memory somewhere!
        if(((*read_iter)->startStopsAt(dr_end_index) - (*read_iter)->startStopsAt(dr_start_index)) == (AL_masterDRLength - 1))
        {
            // the start of the read is the position of the master DR - the position of the DR in the read
            int this_read_start_pos = AL_Offsets.at(AL_masterDRToken) - (*read_iter)->startStopsAt(dr_start_index);
            AL_ZoneStart =  this_read_start_pos + (*read_iter)->startStopsAt(dr_start_index);
            AL_ZoneEnd =  this_read_start_pos + (*read_iter)->startStopsAt(dr_end_index);
            break;
        }
    }
}












