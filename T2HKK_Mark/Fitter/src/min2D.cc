#include <stdlib.h>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "math.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TGraph.h"
#include "TVectorD.h"
#include "TRandom3.h"
#include "TVirtualFitter.h"
#include "TFitter.h"
#include "TMinuit.h"
#include "TF1.h"
#include "TMath.h"
#include "MakeHistograms.h"
#include "OscParams.h"
#include "SystParams.h"
#include <omp.h>


//Input function and variables
void ParseArgs(int argc, char **argv);
char *fCardFileHK = NULL;
char *fCardFileKD = NULL;
char *fCardOsc = NULL;
char *fCardSyst = NULL;
char *fOutFile = NULL;
//dm32, ssqth23, dm21, ssqth13, ssqth12, dcp
char *fName1 = NULL;
char *fName2 = NULL;
int fFixHierarchy;
float fParam1, fParam2;

//Function to calculate the likelihood
double CalcLogLikelihood(TH1D **data1Re, TH1D **data1Rmu, TH1D **pred1Re, TH1D **pred1Rmu, bool cut = true);

//FCN for fitter
void myFcn(Int_t & /*nPar*/, Double_t * /*grad*/ , Double_t &fval, Double_t *p, Int_t /*iflag */  );

//Instances of class to make histograms
MakeHistograms *histsHK=NULL;
MakeHistograms *histsKD=NULL;

//Data and predicted histograms
TH1D *data1ReHK[2];
TH1D *data1RmuHK[2];
TH1D *pred1ReHK[2];
TH1D *pred1RmuHK[2];
TH1D *data1ReKD[2];
TH1D *data1RmuKD[2];
TH1D *pred1ReKD[2];
TH1D *pred1RmuKD[2];

//Oscillation probability
ProbWrapper* prob;

//Oscillation parameters
OscParams* oscParams;

//Systematic parameters
SystParams* systParams;

int main(int argc, char *argv[])
{

 //Initialize variabels
 fFixHierarchy = 1;

 //Get the arguments
 ParseArgs(argc, argv);

 //Set All Histograms to NULL
 for(int i=0; i<2; i++){
   data1ReHK[i] = NULL; 
   data1RmuHK[i] = NULL; 
   data1ReKD[i] = NULL; 
   data1RmuKD[i] = NULL; 
   pred1ReHK[i] = NULL;
   pred1RmuHK[i] = NULL;
   pred1ReKD[i] = NULL;
   pred1RmuKD[i] = NULL; 
 } 

 //Oscillation probability
 prob = new ProbWrapper();
 prob->SetDeltaCP(0);
 prob->SetBaseLine(1100.);
 prob->SetDensity(3.0);

 //Class for building histograms
 histsHK = new MakeHistograms(fCardFileHK, prob, 0);
 if(fCardFileKD!=NULL)
   histsKD = new MakeHistograms(fCardFileKD, prob, 1);

 //Load the oscillation parameters
 oscParams = new OscParams(fCardOsc);

 //Set the oscillation parameters and settings to calculate the prediction
 prob->SetOscillationParameters(oscParams->GetParamValue(0),oscParams->GetParamValue(1),oscParams->GetParamValue(2),
                                oscParams->GetParamValue(3),oscParams->GetParamValue(4),oscParams->GetParamValue(5));
 prob->SetBaseLine(histsHK->GetBaseline());
 prob->SetDensity(histsHK->GetDensity());

 //Make the first detector (HK) fake data
 histsHK->ApplyFluxWeights();
 histsHK->ApplyOscillations();
 histsHK->GetPredictions(data1ReHK, data1RmuHK, systParams);

 //Only do the second detector if it is specified 
 if(histsKD!=NULL){
   //Change the baseline and matter density
   prob->SetBaseLine(histsKD->GetBaseline());
   prob->SetDensity(histsKD->GetDensity());
 
   //Make the second detector (KD) fake data
   histsKD->ApplyFluxWeights();
   histsKD->ApplyOscillations();
   histsKD->GetPredictions(data1ReKD, data1RmuKD, systParams);
 }

 //Starting points for profiled oscillation parameters
 int npoints[6] = {1,2,1,1,1,16};
 float startPoints[6][16];
 startPoints[0][0] = oscParams->GetParamValue(0);
 if(fFixHierarchy == 0){
   startPoints[0][1] = -1.0*oscParams->GetParamValue(0);
   npoints[0] = 2;
 }
 startPoints[1][0] = 0.4;
 startPoints[1][1] = 0.6;
 startPoints[2][0] = oscParams->GetParamValue(2);
 startPoints[3][0] = oscParams->GetParamValue(3);
 startPoints[4][0] = oscParams->GetParamValue(4);
 for(int i=0; i<16; i++) startPoints[5][i] = (float)i*2.0*TMath::Pi()/16.0;

 //save the information of the fixed parameters
 bool fixedParam[6] = {false, false, false, false, false, false};
 char pnames[6][20] = {"dm32", "ssqth23", "dm21", "ssqth13", "ssqth12", "dcp"};
 for(int i=0; i<6; i++){
   if( strcmp(pnames[i],fName1)==0 ){
     oscParams->SetParamValue(i,fParam1);
     fixedParam[i] = true;
     npoints[i] = 1;
     startPoints[i][0] = fParam1;
   }
   if( strcmp(pnames[i],fName2)==0 ){
     oscParams->SetParamValue(i,fParam2);
     fixedParam[i] = true;
     npoints[i] = 1;
     startPoints[i][0] = fParam2;
   }
 }


 //Load the systematic parameters 
 systParams = new SystParams(fCardSyst);

 //Use a minuit fitter
 TFitter *minuit = new TFitter(oscParams->GetNOsc()+systParams->GetNSysts());

 //Make the output ttree
 TFile *fout = new TFile(fOutFile,"RECREATE");
 TTree results("results","Minimum with 2 parameters fixed");
 float minimum=9e9;
 std::string pname1(fName1);
 std::string pname2(fName2);
 results.Branch("Minimum",&minimum,"Minimum/F");
 results.Branch("P1Value",&fParam1,"P1Value/F");
 results.Branch("P2Value",&fParam2,"P2Value/F");
 results.Branch("P1Name",&pname1);
 results.Branch("P2Name",&pname2);

 int ia[6];
 for(ia[0]=0; ia[0]<npoints[0]; ia[0]++)
 for(ia[1]=0; ia[1]<npoints[1]; ia[1]++)
 for(ia[2]=0; ia[2]<npoints[2]; ia[2]++)
 for(ia[3]=0; ia[3]<npoints[3]; ia[3]++)
 for(ia[4]=0; ia[4]<npoints[4]; ia[4]++)
 for(ia[5]=0; ia[5]<npoints[5]; ia[5]++){

   //Clear the fitter 
   minuit->Clear(); 

   int piter = 0;
   //Set Systematic Parameters
   for(int i=0; i<systParams->GetNSysts(); i++){
     minuit->SetParameter(piter,Form("syst%d",i),systParams->GetParamNominal(i),systParams->GetParamError(i)/10.,0,0);
     piter++;
   }

   //Set Oscillation parameters
   for(int i=0; i<oscParams->GetNOsc(); i++){
     if(oscParams->GetParamType(i)==0)
        minuit->SetParameter(piter,Form("osc%d",i),startPoints[i][ia[i]],oscParams->GetParamError(i)/100.,
                            (i==0 && startPoints[i][ia[i]]<0. && oscParams->GetParamMinimum(i)>0. ? -1.0*oscParams->GetParamMaximum(i) : oscParams->GetParamMinimum(i)),
                            (i==0 && startPoints[i][ia[i]]<0. && oscParams->GetParamMaximum(i)>0. ? -1.0*oscParams->GetParamMinimum(i) : oscParams->GetParamMaximum(i)));
     else
        minuit->SetParameter(piter,Form("osc%d",i),startPoints[i][ia[i]],oscParams->GetParamError(i)/100.,oscParams->GetParamMinimum(i),oscParams->GetParamMaximum(i));
     if(fixedParam[i]) minuit->FixParameter(piter);
     piter++;
   }  

   //Set the FCN
   minuit->SetFCN(myFcn);
          
   // set print level
   double arglist[100];
   arglist[0] = 2;
   minuit->ExecuteCommand("SET PRINT",arglist,2);

   //Set the minimizer
   arglist[0] = 50000; // number of function calls
   arglist[1] = 0.001; // tolerance
   minuit->ExecuteCommand("MIGRAD",arglist,2);
   //minuit->ExecuteCommand("MINI",arglist,2);

   //Get the fit results
   double amin,edm,errdeff;
   int nvpar, nparx;
   int conv = minuit->GetStats(amin,edm,errdeff,nvpar,nparx);

   //If the fit converged and the minimum is less than other starting points, save it
   if(amin<minimum && conv==3) minimum = amin;
             
 }
  
 //Fill the ttree
 results.Fill();

 //Write the ttree to the output file
 results.Write();
 fout->Close();
}

//Function to parse the input arguments
void ParseArgs(int argc, char **argv){
  std::cout << "parse args" << std::endl;
  int nargs = 1; 
  if(argc<(nargs*2+1)){ exit(1); }
  for(int i = 1; i < argc; i++){
    if(std::string(argv[i]) == "-c1") fCardFileHK = argv[++i];
    if(std::string(argv[i]) == "-c2") fCardFileKD = argv[++i];
    if(std::string(argv[i]) == "-co") fCardOsc = argv[++i];
    if(std::string(argv[i]) == "-cs") fCardSyst = argv[++i];
    if(std::string(argv[i]) == "-o") fOutFile = argv[++i];
    if(std::string(argv[i]) == "-p1") fParam1 = atof(argv[++i]);
    if(std::string(argv[i]) == "-p2") fParam2 = atof(argv[++i]);
    if(std::string(argv[i]) == "-n1") fName1 = argv[++i];
    if(std::string(argv[i]) == "-n2") fName2 = argv[++i];
    if(std::string(argv[i]) == "-fh") fFixHierarchy = atoi(argv[++i]);
  } 
}

//Function to calculate the likelihood
double CalcLogLikelihood(TH1D **data1Re, TH1D **data1Rmu, TH1D **pred1Re, TH1D **pred1Rmu, bool cut){

  double ll = 0.;
  for(int i=0; i<2; i++)
    for(int j=1; j<data1Re[i]->GetNbinsX(); j++){
      double erec = data1Re[i]->GetXaxis()->GetBinCenter(j);
      if(erec>1.2 && cut) continue;
      if(pred1Re[i]->GetBinContent(j)>1.0e-9 && data1Re[i]->GetBinContent(j)>1.0e-9)
        ll += 2.0*(pred1Re[i]->GetBinContent(j)-data1Re[i]->GetBinContent(j)+
              data1Re[i]->GetBinContent(j)*TMath::Log( data1Re[i]->GetBinContent(j)/pred1Re[i]->GetBinContent(j)) );
      else if(data1Re[i]->GetBinContent(j)<=1.0e-9)
        ll += 2.0*pred1Re[i]->GetBinContent(j);
    } 

  for(int i=0; i<2; i++)
    for(int j=1; j<data1Rmu[i]->GetNbinsX(); j++){
      double erec = data1Rmu[i]->GetXaxis()->GetBinCenter(j);
      if(pred1Rmu[i]->GetBinContent(j)>1.0e-9 && data1Rmu[i]->GetBinContent(j)>1.0e-9)
        ll += 2.0*(pred1Rmu[i]->GetBinContent(j)-data1Rmu[i]->GetBinContent(j)+
              data1Rmu[i]->GetBinContent(j)*TMath::Log( data1Rmu[i]->GetBinContent(j)/pred1Rmu[i]->GetBinContent(j)) );
      else if(data1Rmu[i]->GetBinContent(j)<=1.0e-9)
        ll += 2.0*pred1Rmu[i]->GetBinContent(j);
    } 

  return ll;
}

//FCN function for the minimizer
void myFcn(Int_t & /*nPar*/, Double_t * /*grad*/ , Double_t &fval, Double_t *p, Int_t /*iflag */  ){


  double matterDensity[2];
  matterDensity[0] = histsHK->GetDensity();
  if(histsKD!=NULL) matterDensity[1] = histsKD->GetDensity();

  //Get the updated systematic parameter values
  for(int i=0; i<systParams->GetNSysts(); i++){
    systParams->SetParamValue(i,p[i]);
    if(systParams->GetParamType(i)==MATTERDENSITY){
      if(systParams->DoDetector(1,i)) matterDensity[0] = p[i];
      if(systParams->DoDetector(2,i)) matterDensity[1] = p[i];
    }
  }
  int nSyst = systParams->GetNSysts();

  //Update the oscillation parameter values
  prob->SetOscillationParameters(p[nSyst],p[nSyst+1],p[nSyst+2],p[nSyst+3],p[nSyst+4],p[nSyst+5]);
  //Calculate the first detector prediction
  prob->SetBaseLine(histsHK->GetBaseline());
  prob->SetDensity(matterDensity[0]);
  histsHK->ApplyOscillations();
  histsHK->GetPredictions(pred1ReHK, pred1RmuHK,systParams);
  if(histsKD!=NULL){
    //Calculate the secon detector prediction
    prob->SetBaseLine(histsKD->GetBaseline());
    prob->SetDensity(matterDensity[1]);
    histsKD->ApplyOscillations();
    histsKD->GetPredictions(pred1ReKD, pred1RmuKD,systParams);
  }

  //Get the data chi2
  double chisq = 0.;
  chisq += CalcLogLikelihood(data1ReHK,data1RmuHK,pred1ReHK, pred1RmuHK, (histsHK->GetBaseline()<300.) );
  if(histsKD!=NULL) chisq += CalcLogLikelihood(data1ReKD,data1RmuKD,pred1ReKD, pred1RmuKD, (histsKD->GetBaseline()<300.) );

  //Get the oscillation parameter penalty term
  for(int i=0; i<oscParams->GetNOsc(); i++)
    if(oscParams->GetParamType(i)==1)
      chisq += pow( (p[i+nSyst]-oscParams->GetParamValue(i))/oscParams->GetParamError(i), 2);

  //Get the systematic parameter penalty term
  chisq += systParams->GetChi2();

  fval = chisq;

}

