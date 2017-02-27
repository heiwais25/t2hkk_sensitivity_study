{

  TChain *chain = new TChain("results");

  for(int i=0; i<101; i++) chain->AddFile(Form("2det_hk_total/cp_prec_2det_hk_0p25x_total_%d.root",i));

  TFile *fout = new TFile("2det_hk_total/cp_prec_2det_hk_0p25x_total_combine.root","RECREATE");
  TTree *tree = chain->CloneTree();
  tree->Write("results");
  fout->Close();

}
  
