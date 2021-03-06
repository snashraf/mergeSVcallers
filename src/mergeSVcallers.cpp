/*

This program was created at:  Tue Dec  1 11:51:06 2015
This program was created by:  Zev N. Kronenberg

Contact: zev.kronenber@gmail.com

Organization: University of washington\n Genome Sciences

The MIT License (MIT)

Copyright (c) <2015> <Zev N. Kronenberg>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

qThe above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.


*/

#include <string>
#include <iostream>
#include <math.h>
#include <cmath>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <ctime>
#include "../vcflib/src/split.h"
#include "../vcflib/include/Fasta.h"
#include "../vcflib/include/Variant.h"

int min(int l, int r){
  if(r < l){
    return r;
  }
  return l;
}

int max(int l, int r){
  if(r < l){
    return l;
  }
  return r;
}

int unionInterval(int s1, int s2, int e1, int e2){

  int ui = max(e1, e2)  - min(s1, s2);

  return ui;
}

int intersectionInterval(int s1, int s2, int e1, int e2){

  int ii =  min(e1, e2) - max(s1, s2);
  return ii;

}

double reciprocalOverlap(int s1, int s2, int e1, int e2){

  return double(intersectionInterval(s1, s2, e1, e2))/double(unionInterval(s1,s2,e1,e2));

}

// sorts vcflib::Variant by start;
bool sortStart(vcflib::Variant * L, vcflib::Variant * R){

  if(L->sequenceName != R->sequenceName){
    std::cerr << "FATAL: trying to sort positions on different chromosomes." << std::endl;
    exit(1);
  }

  return (L->position < R->position);
}

struct varContainer{
  bool processed;
  vcflib::Variant * var;
};

struct options{
  int                                maxDist;
  double                          reciprocal;
  std::vector<std::string>             files;
  std::vector<std::string>              tags;
  std::map<std::string, int>          seqids;
  std::map<std::string, int>            skip;
  std::vector<vcflib::VariantCallFile*> vcfs;
  std::string                         fastaF;
  FastaReference                      fastaH;

}globalOpts;

static const char *optString = "hf:t:a:s:r:";

void printHelp(void){
  cerr << " Usage:  " << endl;
  cerr << "       mergeSVcallers -a ref.fasta -f a.vcf.gz,b.vcf.gz -t WHAM,LUMPY -s 500" << endl;
  cerr << endl;
  cerr << " Required:  " << endl;
  //------------------------------- XXXXXXXXXX --------------------------------
  std::cerr << "          -a - <STRING> - The samtools faidx indexed FASTA file" << std::endl;
  std::cerr << "          -f - <STRING> - A comma separated list of Tabix indexed VCF files" << endl;
  std::cerr << "          -t - <STRING> - A comma separated  list of tags/identifiers for each file" << endl;

  std::cerr << endl;
  std::cerr << " Optional:  " << endl;
  std::cerr << "          -s - <INT>   - Merge SVs with both breakpoints N BP away [100] " << std::endl;
  std::cerr << "          -r - <FLOAT> - Reciprocal overlap also required  [0]           " << std::endl;
  std::cerr << " Info:  " << endl;
  std::cerr << "          -This tool provides a simple set of operations to merge SVs." << std::endl;
  std::cerr << "          -Output is unsorted." << std::endl;
  std::cerr << endl;
  //  printVersion();
}

//-------------------------------   OPTIONS   --------------------------------
int parseOpts(int argc, char** argv)
{
  int opt = 0;
  opt = getopt(argc, argv, optString);
  while(opt != -1){
    switch(opt){
    case 'r':
      {
	globalOpts.reciprocal = atof(optarg);
	std::cerr << "INFO: reciprocal overlap required: " << globalOpts.reciprocal << std::endl;
	break;
      }
    case 's':
      {
	globalOpts.maxDist = atoi(optarg);
	std::cerr << "INFO: maxDist: " << globalOpts.maxDist << std::endl;
	break;
      }
    case 'a':
      {
	globalOpts.fastaF = optarg;
	globalOpts.fastaH.open(globalOpts.fastaF);
	std::cerr << "INFO: ref: " << globalOpts.fastaF << std::endl;

	for(vector<std::string>::iterator it =
	      globalOpts.fastaH.index->sequenceNames.begin();
	    it != globalOpts.fastaH.index->sequenceNames.end(); it++){

	  globalOpts.seqids[*it] = 1;
	}
	break;
      }
    case 'h':
      {
	printHelp();
	exit(1);
	break;
      }
    case 'f':
      {
	globalOpts.files = split(optarg, ",");
	for(std::vector<std::string>::iterator it = globalOpts.files.begin();
	    it != globalOpts.files.end(); it++){
	  std::cerr << "INFO: file: " << *it << std::endl;
	  vcflib::VariantCallFile * vcf;
	  vcf =  new vcflib::VariantCallFile;
	  vcf->open(*it);
	  globalOpts.vcfs.push_back(vcf);
	}
	break;
      }
    case 't':
      {
	globalOpts.tags = split(optarg, ",");
	for(std::vector<std::string>::iterator it = globalOpts.tags.begin();
	    it != globalOpts.tags.end(); it++){
	  std::cerr << "INFO: tag: " << *it << std::endl;
	}
	break;
      }
    case '?':
      {
	break;
      }
    }
    opt = getopt( argc, argv, optString );
  }
  return 1;
}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : void

 Function does   : prints a header

 Function returns: nothing

*/

void printHeader(void){

  time_t t = time(NULL);
  tm* timePtr = localtime(&t);

  stringstream ss;
  ss << "##fileformat=VCFv4.2" << std::endl;
  ss << "##fileDate=" << (timePtr->tm_year + 1900) << timePtr->tm_mon << timePtr->tm_mday << std::endl;
  ss << "##source=mergeSVcallers" << std::endl;
  ss << "##reference=file:" << globalOpts.fastaF << std::endl;
  ss << "##INFO=<ID=SVTYPE,Number=1,Type=String,Description=\"Type of structural variant\">" << std::endl;
  ss << "##INFO=<ID=END,Number=1,Type=Integer,Description=\"End position of the variant described in this record\">" << std::endl;
  ss <<  "##INFO=<ID=SVLEN,Number=.,Type=Integer,Description=\"Difference in length between REF and ALT alleles\">" << std::endl;
  ss << "##INFO=<ID=CIPOS,Number=2,Type=Integer,Description=\"Confidence interval around POS for imprecise variants\">"  << std::endl;
  ss << "##INFO=<ID=CIEND,Number=2,Type=Integer,Description=\"Confidence interval around END for imprecise variants\">" << std::endl;
  ss << "##INFO=<ID=FIVE,Number=.,Type=Integer,Description=\"All POS collapsed into svcall\">" << std::endl;
  ss << "##INFO=<ID=THREE,Number=.,Type=Integer,Description=\"All END collapsed into svcall\">" << std::endl;
  ss << "##INFO=<ID=TAGS,Number=.,Type=String,Description=\"all tags collapsed ino record\">" << std::endl;
  ss << "##INFO=<ID=UTAGS,Number=.,Type=String,Description=\"Unique tags\">" << std::endl;
  ss << "##INFO=<ID=NCOL,Number=1,Type=Integer,Description=\"The number of merged records\">" << std::endl;
  ss << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT\tNONE" << std::endl;

  std::cout << ss.str();

}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector of strings and separator

 Function does   : joins vector with separator

 Function returns: string

*/

string join(vector<int> & ints, string sep){
  stringstream ss ;

  ss << ints.front();

  for(uint i = 1 ; i < ints.size(); i++){
    ss << sep << ints[i];
  }

  return ss.str();

}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector of strings and separator

 Function does   : joins vector with separator

 Function returns: string

*/

string join(vector<string> & strings, string sep){

  string joined = *(strings.begin());

  for(vector<string>::iterator sit = strings.begin()+1;
      sit != strings.end(); sit++){

    joined = joined + sep + (*sit) ;
  }
  return joined;
}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector of doubles

 Function does   : calculates the var

 Function returns: double

*/

double var(vector<int> & data, double mu){
  double variance = 0;

  for(vector<int>::iterator it = data.begin(); it != data.end(); it++){
    variance += pow((*it) - mu,2);
  }

  if(variance == 0){
    return 0;
  }

  return variance / (data.size() - 1);
}


//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector of ints

 Function does   : calculates the mean

 Function returns: double

*/

double mean(vector<int> & data){

  double sum = 0;

  for(vector<int>::iterator it = data.begin(); it != data.end(); it++){
    sum += (*it);
  }
  return sum / data.size();
}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector vcflib::Variant pointers

 Function does   : loads the end positions;

 Function returns: void

*/

void getEnds(vector<vcflib::Variant *> & svs, std::vector<int> & ends){

  for(vector<vcflib::Variant *>::iterator it = svs.begin();
      it != svs.end(); it++){

    if((*it)->info.find("THREE") != (*it)->info.end()){

      for(std::vector<std::string>::iterator iz = (*it)->info["THREE"].begin();
	  iz != (*it)->info["THREE"].end(); iz++){
	ends.push_back(atoi((*iz).c_str()));
      }
    }
    else{
      ends.push_back(atoi((*it)->info["END"].front().c_str()));
    }
  }
}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector vcflib::Variant pointers

 Function does   : loads the start positions;

 Function returns: void

*/

void getStarts(vector<vcflib::Variant *> & svs, std::vector<int> & starts){

  for(vector<vcflib::Variant *>::iterator it = svs.begin();
      it != svs.end(); it++){

    if((*it)->info.find("FIVE") != (*it)->info.end()){

      for(std::vector<std::string>::iterator iz = (*it)->info["FIVE"].begin();
	  iz != (*it)->info["FIVE"].end(); iz++){
	starts.push_back(atoi((*iz).c_str()));
      }
    }
    else{
      starts.push_back((*it)->position);
    }
  }
}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector vcflib::Variant pointers

 Function does   : grabs a tag and loads it into a std::vector<std::string>

 Function returns: void

*/

bool  getTag(vector<vcflib::Variant *> & svs,
	    std::vector<std::string> & source, std::string  tag){

  bool flag = false;

  for(vector<vcflib::Variant *>::iterator it = svs.begin();
      it != svs.end(); it++){

    if((*it)->info.find(tag) != (*it)->info.end()){
      flag = true;

      for(std::vector<std::string>::iterator iz = (*it)->info[tag].begin();
	  iz != (*it)->info[tag].end(); iz++){
	source.push_back(*iz);
      }
    }
  }
  return flag;
}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector vcflib::Variant pointers

 Function does   : Joins and prints the SVs

 Function returns: void

*/

void mergeAndDump(vector<vcflib::Variant *> & svs){

  vector<int>             starts;
  vector<int>               ends;
  vector<std::string>      tools;
  map<std::string, int> uniqTags;
  std::vector<std::string> uTags;
  vector<std::string>      ncols;

  std::string seqid  = svs.front()->sequenceName;
  std::string svtype = svs.front()->info["SVTYPE"].front();

  getStarts(svs, starts);
  getEnds(svs,     ends);
  if(getTag(svs, tools, "TAGS")){
    for(std::vector<std::string>::iterator it = tools.begin();
	it != tools.end(); it++){
      uniqTags[*it] = 1;
    }
    for(std::map<std::string, int>::iterator it = uniqTags.begin();
	it != uniqTags.end(); it++){
      uTags.push_back(it->first);
    }

  }
  else{
    tools.push_back(".");
  }

  double RPOS = mean(starts);
  double REND = mean(ends  );

  double fiveSD  = sqrt(var(starts,   RPOS));
  double threeSD = sqrt(var(ends,     REND));

  int POS = (int)RPOS;
  int END = (int)REND;

  int fiveCIL = -(int)(fiveSD*2);
  int fiveCIH = -fiveCIL;

  if(fiveCIL > -10){
    fiveCIL = -10;
  }
  if(fiveCIH < 10){
    fiveCIH = 10;
  }

  int threeCIL = -(int)(threeSD*2);
  int threeCIH = -threeCIL;

  if(threeCIL > -10){
    threeCIL = -10;
  }
  if(threeCIH < 10){
    threeCIH = 10;
  }

  stringstream cipos;
  stringstream ciend;

  cipos << "CIPOS="  << fiveCIL << "," << fiveCIH << ";";
  ciend << "CIEND="  << threeCIL << "," << threeCIH << ";";

  string ciPOS;
  string ciEND;

  if(POS > END){
    int TMP = END;
    END = POS;
    POS = TMP;
    ciPOS = ciend.str();
    ciEND = cipos.str();
  }
  else{
    ciPOS = cipos.str();
    ciEND = ciend.str();
  }

  int NCOL = starts.size();
  if(NCOL == 1){
    NCOL -= 1;
  }
  std::string ID     = ".";
  std::string QUAL   = ".";
  std::string FILTER = ".";


  std::string REF = globalOpts.fastaH.getSubSequence(seqid, POS, 1);

  stringstream line;

  int SVLEN = (END - POS);

  if(svtype == "DEL"){
    SVLEN = -SVLEN;
  }


  line << seqid     << "\t"
       << POS       << "\t"
       << ID        << "\t"
       << REF       << "\t"
       << "<"       << svtype << ">" << "\t"
       << QUAL      << "\t"
       << FILTER    << "\t"
       << "SVTYPE=" << svtype << ";"
       << "END="    << END    << ";"
       << "SVLEN="  << SVLEN  << ";"
       << ciPOS
       << ciEND
       << "FIVE="   << join(starts, ",") << ";"
       << "THREE="  << join(ends, ",") << ";"
       << "TAGS="   << join(tools, ",") << ";"
       << "UTAGS="  << join(uTags, ",") << ";"

       << "NCOL="   << NCOL
       << "\t.\t.";



  std::cout << line.str() << std::endl;

}


//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : vector vcflib::Variant pointers

 Function does   : The bool is used to overload for SV type.
		   This function groups the SVs by type.
 Function returns: void

*/

void mergeAndDump(vector<vcflib::Variant *> & svs, bool){
  map<string, vector< vcflib::Variant *> > typeGroup;
  for(vector<vcflib::Variant *>::iterator iz = svs.begin(); iz != svs.end(); iz++){
    typeGroup[(*iz)->info["SVTYPE"].front()].push_back(*iz);
  }

  for(map<string, vector< vcflib::Variant *> >::iterator iz = typeGroup.begin();
      iz != typeGroup.end(); iz++){
    mergeAndDump(iz->second);
  }
}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : a vector of var objects
 Function does   : processes a chunk of the files
 Function returns:
*/

void manageLoopOverVar(std::vector<vcflib::Variant *> & data){

    std::vector<vcflib::Variant *> tmpdata;
    for(std::vector<vcflib::Variant *>::iterator it = data.begin();
        it != data.end(); it++ ){
        tmpdata.push_back(*it);
    }

    bool moreToDump = true;

    std::vector<vcflib::Variant *> svBuffer;

    while(moreToDump){
        // current variant
        vcflib::Variant * cp = tmpdata.front();
        /* we are searching for the first non empty vcflib obj
           if we do not find anything we are done             */
        int i = 0;
        while(cp == NULL){
            i += 1;
            if(i == tmpdata.size()){
                moreToDump = false;
                break;
            }
            cp = tmpdata[i];
        }
        if(cp == NULL){
            break;
        }
        /* we loop over variants to see if there are any to
           merge with cp and put them in the buffer */
        svBuffer.push_back(cp);

        for(std::vector<vcflib::Variant *>::iterator it = tmpdata.begin();
            it != tmpdata.end(); it++ ){
            // no self merging
            if(*it == NULL){
                continue;
            }
            if(*it == cp){
                *it = NULL;
                continue;
            }
            if(atoi(cp->info["END"].front().c_str()) <
               (*it)->position){
                break;
            }

            int d1 = abs(cp->position - (*it)->position );
            int d2 = abs(
                         atoi(cp->info["END"].front().c_str())  -
                         atoi((*it)->info["END"].front().c_str() )
                         );
            if(d1 > globalOpts.maxDist || d2 > globalOpts.maxDist){
                continue;
            }
            double recip = reciprocalOverlap(cp->position,
                                             (*it)->position,
                                             atoi(cp->info["END"].front().c_str()),
                                             atoi((*it)->info["END"].front().c_str()));

            if( recip < globalOpts.reciprocal){
                continue;
            }
            svBuffer.push_back(*it);
            *it = NULL;
        }

        cp = NULL;
        mergeAndDump(svBuffer, true);

        svBuffer.clear();
    }
    // cleanup
    for(std::vector<vcflib::Variant *>::iterator it = data.begin();
        it != data.end(); it++ ){
        delete (*it);
    }
}

//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : a seqid

 Function does   : processes a chunk of the files

 Function returns:

*/

void processChunk(std::string  seqid){

  std::vector<vcflib::Variant *> data;

  int index = 0;

  // loading the data into the vector
  for(std::vector<vcflib::VariantCallFile*>::iterator it = globalOpts.vcfs.begin();
      it != globalOpts.vcfs.end(); it++){



    bool getNext = true;

    vcflib::Variant var(**it);

    if(!(*it)->setRegion(seqid)){
      std::cerr << "WARNING: could not set region: seqid: " << seqid << " file:  " << globalOpts.files[index] << std::endl;
      std::cerr << "INFO: Seqid might not be in file" << std::endl;
    }

    // looping over read and loading it into new vcflib pointer

    while (getNext) {
      if(!(*it)->getNextVariant(var)){
	getNext = false;
      }
      else{

	// remapping genomestrip SV calls

	if(var.info["SVTYPE"].front() == "CNV"){
	  if(var.info.find("GSCNCATEGORY") != var.info.end()){
	    var.info["SVTYPE"] = var.info["GSCNCATEGORY"];

	    if(var.info["SVTYPE"].front() == "MIXED"){
	      var.info["SVTYPE"][0] = "CNV";
	    }
	  }
	}

	if(var.info["SVTYPE"].front() == "BND"){
	  continue;
	}
	if(var.info["SVTYPE"].front() == "TRA"){
	  continue;
	}

	vcflib::Variant * v  = new vcflib::Variant;
	*v = var;
	if(v->info["TAGS"].empty()){
	  v->info["TAGS"].push_back(globalOpts.tags[index]);
	  data.push_back(v);
	}
	else{
	  data.push_back(v);
	}
      }
    }
    index += 1;
  }

  std::cerr << "INFO: sorting: seqid: " << seqid << std::endl;
  std::sort(data.begin(), data.end(), sortStart);

  std::cerr << "n SVs in chunk: " << data.size() << std::endl;

  if(data.empty()){
    return;
  }

  manageLoopOverVar(data);

}


//------------------------------- SUBROUTINE --------------------------------
/*
 Function input  : reads from globalOpts

 Function does   : validates that the VCF lines are okay;

 Function returns:

*/
void validate (void)
{

  int index = 0;


  for(std::vector<vcflib::VariantCallFile*>::iterator it = globalOpts.vcfs.begin(); it != globalOpts.vcfs.end(); it++){

    std::cerr << "INFO: validating: " << globalOpts.files[index] << std::endl;

    if (!(*it)->is_open()) {
      std::cerr << "FATAL: could not open one of the VCF files" << std::endl;
      exit(1);
    }
    vcflib::Variant var(**it);

    int nvar = 0;

    while ( (*it)->getNextVariant(var) ) {

      nvar += 1;

      if(var.info.find("SVTYPE") == var.info.end()){
	std::cerr << "FATAL: missing SVTYPE in: " << globalOpts.files[index] << " : " << var.originalLine
		  << std::endl;
	exit(1);
      }

      if(var.info["SVTYPE"].front() ==  "BND"){
	std::cerr << "WARNING: BND events are skipped: " << globalOpts.files[index] << std::endl;
	continue;
      }

      if(var.info["SVTYPE"].front() ==  "TRA"){
	std::cerr << "WARNING: TRA events are skipped: " << globalOpts.files[index] << std::endl;
	continue;
      }
      if(var.info.find("END") == var.info.end()){
	std::cerr << "FATAL: missing END in: " << globalOpts.files[index] << " : " << var << std::endl;
	exit(1);
      }

      if(atoi(var.info["END"].front().c_str()) < var.position ){
	std::cerr << "FATAL: END < POS : " << globalOpts.files[index] << " : " << var << std::endl;
	exit(1);
      }

      if(var.info.find("SVTYPE") == var.info.end()){
	std::cerr << "FATAL: missing SVTYPE in: " << globalOpts.files[index] << std::endl;
	exit(1);
      }

      if(nvar == 1000){
	break;
      }
    }

    std::cerr << "INFO: " << globalOpts.files[index] << " - VCF INFO: END   [x]" << std::endl;
    std::cerr << "INFO: " << globalOpts.files[index] << " - VCF INFO: SVTYPE [x]" << std::endl;

    index += 1;

  }
}
//-------------------------------    MAIN     --------------------------------
/*
 Comments:
*/

int main( int argc, char** argv)
{
  globalOpts.reciprocal = 0;
  globalOpts.maxDist  = 100;

  parseOpts(argc, argv);

  if(globalOpts.fastaF.empty()){
    std::cerr << "FATAL: required: -a - fasta file" << std::endl;
    printHelp();
    exit(1);
  }
  if(globalOpts.tags.empty()){
    std::cerr << "FATAL: required: -t - tags for input files" << std::endl;
    printHelp();
    exit(1);
  }
  if(globalOpts.tags.size() != globalOpts.files.size()){
    std::cerr << "FATAL: The number of tags and files are different" << std::endl;
    printHelp();
    exit(1);
  }

  validate();
  std::cerr << "INFO: All VCF files validated" << std::endl;

  printHeader();

  for(std::map<std::string, int>::iterator it = globalOpts.seqids.begin();
      it != globalOpts.seqids.end(); it++){

    processChunk(it->first);
  }

  std::cerr << "INFO: mergeSVcallers finished" << std::endl;

  return 0;
}
