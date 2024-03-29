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
double density_KD;
double default_pot1;
double default_pot2;
int fHierarchy;
int fDeltacp;
int fYear;

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
    fHierarchy  = -1;
    fDeltacp    = -1; 
    fYear       = -1;
    default_pot1 = -1;
    default_pot2 = -1;
    density_KD = -1;

    //Get the arguments
    ParseArgs(argc, argv);

    //Set All Histograms to NULL
    for(int i=0; i<2; i++)
    {
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

    //Load the oscillation parameters
    oscParams = new OscParams(fCardOsc);

    //Load the systematic parameters 
    systParams = new SystParams(fCardSyst);
    double JD_pot1, JD_pot2;
    double KD_pot1, KD_pot2;



    //Class for building histograms
    histsHK = new MakeHistograms(fCardFileHK, prob, 0);
    //histsHK->GetPOTWeight(&default_pot1, &default_pot2);
    JD_pot1 = default_pot1 * 4 * double(fYear) / 100.;
    JD_pot2 = default_pot2 * 4 * double(fYear) / 100.;

    //histsHK->SetPOTWeight(JD_pot1, JD_pot2);
    histsHK->ApplyFluxWeights();

    //histsHK->GetPOTWeight(&default_pot1, &default_pot2);
    //std::cout << default_pot1 << " " << default_pot2 << std::endl;
    if(fCardFileKD!=NULL)
    {
        KD_pot1 = JD_pot1;
        KD_pot2 = JD_pot2;
        histsKD = new MakeHistograms(fCardFileKD, prob, 1);
        density_KD = histsKD->GetDensity(); // Checking the case of 2HK
        //histsKD->SetPOTWeight(KD_pot1, KD_pot2);
        histsKD->ApplyFluxWeights();
    }  


    //Use a minuit fitter
    TFitter *minuit = new TFitter(oscParams->GetNOsc()+systParams->GetNSysts());

    //Make the output ttree
    TFile *fout = new TFile(fOutFile,"RECREATE");
    TTree results("results","CP violation significance");
    float min_known, min_unknown, deltacp;
    int year;
    int hierarchy;
    results.Branch("MinKnown",&min_known,"MinKnown/F");
    results.Branch("MinUnknown",&min_unknown,"MinUnknown/F");
    results.Branch("Year",&year,"Year/I");
    results.Branch("DeltaCP",&deltacp,"DeltaCP/F");
    results.Branch("Hierarchy",&hierarchy,"Hierarchy/I");

    // Set default deltacp value
    fDeltacp = 0;
        //Iterate through the true values of deltacp
    for(int itdcp = 0; itdcp<101; itdcp++)
    {
        //If a value was specified at the command line, only continue if at the value
        if(fDeltacp>=0 && itdcp!=fDeltacp) 
        {
            continue;
        }
        double tdcp = (double)itdcp*(3.14159*2.0/100.);
        deltacp = tdcp;
        year = fYear;
       //Iterate through the hierarchies
        for(int ithier = 0; ithier<2; ithier++)
        {
            //If a value was specified at the command line, only continue if at the value
            if(fHierarchy>=0 && ithier!=fHierarchy) continue;
            hierarchy = ithier;

            //Set the oscillation parameters and settings to calculate the prediction
            prob->SetOscillationParameters((ithier==0 ? 1.0 : -1.0)*oscParams->GetParamValue(0),oscParams->GetParamValue(1),oscParams->GetParamValue(2),
                                            oscParams->GetParamValue(3),oscParams->GetParamValue(4), tdcp );
            prob->SetBaseLine(histsHK->GetBaseline());
            prob->SetDensity(histsHK->GetDensity());

            //Reset the systematics
            systParams->ResetParams();

            //Make the first detector (HK) fake data
            histsHK->ApplyOscillations();
            histsHK->GetPredictions(data1ReHK, data1RmuHK, systParams);

            //Only do the second detector if it is specified 
            if(histsKD!=NULL)
            {
                //Change the baseline and matter density
                prob->SetBaseLine(histsKD->GetBaseline());
                prob->SetDensity(histsKD->GetDensity());

                //Make the second detector (KD) fake data
                histsKD->ApplyOscillations();
                histsKD->GetPredictions(data1ReKD, data1RmuKD, systParams);
            }   
            double check1, check2;
            histsKD->GetPOTWeight(&check1, &check2);
            std::cout << "1 is : " << check1 << " 2 is : " << check2 << std::endl;
            //To save the minima for both hierachies
            double min_fixed = 9e9;
            double min_free = 9e9;

            //Run the fit for both hierarchies
            for(int ihier = 0; ihier<2; ihier++)
            {
                //Start the fit in both octants to ensure global minimum is found
                for(int ioct = 0; ioct<2; ioct++)
                {
                    //Run the fit for both cp conserving states
                    for(int idcp = 0; idcp<2; idcp++)
                    {

                       //Clear the fitter 
                        minuit->Clear(); 

                        int piter = 0;
                       //Set Systematic Parameters
                        for(int i=0; i<systParams->GetNSysts(); i++)
                        {
                            minuit->SetParameter(piter,Form("syst%d",i),systParams->GetParamNominal(i),systParams->GetParamError(i)/10.,0,0);
                            piter++;
                        }

                        //Set Oscillation parameters
                        for(int i=0; i<oscParams->GetNOsc()-1; i++)
                        {
                            double value = oscParams->GetParamValue(i);
                            if(i==0 && ihier==1) value = -1.0*value;
                            if(i==1 && ioct==0) value = 0.40;
                            if(i==1 && ioct==1) value = 0.60;
                            if(oscParams->GetParamType(i)==0)
                            {
                                minuit->SetParameter(piter,Form("osc%d",i),value,oscParams->GetParamError(i)/100.,
                                (i==0 && ihier==1 ? -1.0*oscParams->GetParamMaximum(i) : oscParams->GetParamMinimum(i)),
                                (i==0 && ihier==1 ? -1.0*oscParams->GetParamMinimum(i) : oscParams->GetParamMaximum(i)));
                            }
                            else
                            {
                                minuit->SetParameter(piter,Form("osc%d",i),value,oscParams->GetParamError(i)/100.,oscParams->GetParamMinimum(i),oscParams->GetParamMaximum(i));
                                piter++;
                            }
                        }
                        minuit->SetParameter(piter,Form("osc%d",oscParams->GetNOsc()-1),3.14159*(float)idcp,oscParams->GetParamError(oscParams->GetNOsc()-1)/100.0,
                                                oscParams->GetParamMinimum(oscParams->GetNOsc()-1),oscParams->GetParamMaximum(oscParams->GetNOsc()-1));
                        minuit->FixParameter(piter);

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
                        if(amin<min_free && conv>0) min_free = amin;
                        if(amin<min_fixed && (ihier==ithier) && conv>0) min_fixed = amin;

                    } //dcp loop  
                } //octant loop
            }//hierarchy loop

            //Save the minima and deltachi2
            min_known = min_fixed;
            min_unknown = min_free;

            //Fill the ttree
            results.Fill();
        }  
    }

    //Write the ttree to the output file
    results.Write();
    fout->Close();
}

//Function to parse the input arguments
void ParseArgs(int argc, char **argv)
{
    std::cout << "parse args" << std::endl;
    int nargs = 1; 
    if(argc<(nargs*2+1))
    { 
        exit(1); 
    }
    for(int i = 1; i < argc; i++)
    {
        if(std::string(argv[i]) == "-c1") fCardFileHK = argv[++i];
        if(std::string(argv[i]) == "-c2") fCardFileKD = argv[++i];
        if(std::string(argv[i]) == "-co") fCardOsc = argv[++i];
        if(std::string(argv[i]) == "-cs") fCardSyst = argv[++i];
        if(std::string(argv[i]) == "-o") fOutFile = argv[++i];
        if(std::string(argv[i]) == "-y") fYear = atoi(argv[++i]);

        if(std::string(argv[i]) == "-h") fHierarchy = atoi(argv[++i]);
        if(std::string(argv[i]) == "-d") fDeltacp = atoi(argv[++i]);
    } 
}

//Function to calculate the likelihood
double CalcLogLikelihood(TH1D **data1Re, TH1D **data1Rmu, TH1D **pred1Re, TH1D **pred1Rmu, bool cut)
{

    double ll = 0.;
    for(int i=0; i<2; i++)
    for(int j=1; j<data1Re[i]->GetNbinsX(); j++){
      double erec = data1Re[i]->GetXaxis()->GetBinCenter(j);
      if(erec>1.2 && cut) continue;
      if(erec<0.1) continue;
      if(pred1Re[i]->GetBinContent(j)>1.0e-9 && data1Re[i]->GetBinContent(j)>1.0e-9)
        ll += 2.0*(pred1Re[i]->GetBinContent(j)-data1Re[i]->GetBinContent(j)+
          data1Re[i]->GetBinContent(j)*TMath::Log( data1Re[i]->GetBinContent(j)/pred1Re[i]->GetBinContent(j)) );
    else if(data1Re[i]->GetBinContent(j)<=1.0e-9)
        ll += 2.0*pred1Re[i]->GetBinContent(j);
    } 

    for(int i=0; i<2; i++)
    for(int j=1; j<data1Rmu[i]->GetNbinsX(); j++){
      double erec = data1Rmu[i]->GetXaxis()->GetBinCenter(j);
      if(erec<0.2) continue;
      if(pred1Rmu[i]->GetBinContent(j)>1.0e-9 && data1Rmu[i]->GetBinContent(j)>1.0e-9)
        ll += 2.0*(pred1Rmu[i]->GetBinContent(j)-data1Rmu[i]->GetBinContent(j)+
          data1Rmu[i]->GetBinContent(j)*TMath::Log( data1Rmu[i]->GetBinContent(j)/pred1Rmu[i]->GetBinContent(j)) );
    else if(data1Rmu[i]->GetBinContent(j)<=1.0e-9)
        ll += 2.0*pred1Rmu[i]->GetBinContent(j);
    } 

    return ll;
}

//FCN function for the minimizer
void myFcn(Int_t & /*nPar*/, Double_t * /*grad*/ , Double_t &fval, Double_t *p, Int_t /*iflag */  )
{


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

