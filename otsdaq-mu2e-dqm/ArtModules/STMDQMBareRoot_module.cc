// Author: A. Edmonds (based on P. Murat's TrackerDQM)
// This module produces histograms of data from the STM

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art_root_io/TFileService.h"
#include "fhiclcpp/types/OptionalAtom.h"
#include "canvas/Persistency/Common/TriggerResults.h"
#include "art/Framework/Services/System/TriggerNamesService.h"

#include <TROOT.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TBufferFile.h>
#include <TH1F.h>
#include <TFile.h>
#include <TString.h>
#include <TBufferJSON.h>
#include <TLine.h>
#include <TGraph.h>

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/NetworkUtilities/TCPSendClient.h"
#include "otsdaq-mu2e/ArtModules/HistoSender.hh"

#include "artdaq-core-mu2e/Overlays/STMFragment.hh"

namespace ots {
  class STMDQMBareRoot : public art::EDAnalyzer {
  public:
    struct Config {
      using Name = fhicl::Name;
      using Comment = fhicl::Comment;
      fhicl::Atom<int>             port      { Name("port"),      Comment("This parameter sets the port where the histogram will be sent") };
      fhicl::Atom<std::string>     address   { Name("address"),   Comment("This paramter sets the IP address where the histogram will be sent") };
      fhicl::Atom<std::string>     moduleTag { Name("moduleTag"), Comment("Module tag name") };
      fhicl::Sequence<std::string> histType  { Name("histType"),  Comment("This parameter determines which quantity is histogrammed") };
      fhicl::Atom<int>             freqDQM   { Name("freqDQM"),   Comment("Frequency for sending histograms to the data-receiver") };
      fhicl::Atom<int>             diag      { Name("diagLevel"), Comment("Diagnostic level"), 0 };
      fhicl::Atom<bool>            toJSON    { Name("toJSON"), Comment("Set to tru if sending canvases to a JSON file for online monitoring"), false};
    };

    typedef art::EDAnalyzer::Table<Config> Parameters;

    
    explicit STMDQMBareRoot(Parameters const& conf);

    void analyze(art::Event const& event) override;
    void beginRun(art::Run const&) override;
    void beginJob() override;
    void endJob() override;

  private:
    Config                    conf_;
    int                       port_;
    std::string               address_;
    std::string               moduleTag_;
    std::vector<std::string>  histType_;
    int                       freqDQM_,  diagLevel_, evtCounter_;
    bool _toJSON;

    art::ServiceHandle<art::TFileService> tfs;

    TCanvas* _c_stmdqm;
    TH1F* _evtNumHist;
    TH1F* _dataTypesHist;
    TGraph* _lastWaveform;
    std::fstream  _jsonFile;

    void book_histograms(art::ServiceHandle<art::TFileService> tfs);
    void update_canvas();
    void write_to_json_file();
  };
} // namespace ots

ots::STMDQMBareRoot::STMDQMBareRoot(Parameters const& conf)
  : art::EDAnalyzer(conf), conf_(conf()), port_(conf().port()), address_(conf().address()),
    moduleTag_(conf().moduleTag()), histType_(conf().histType()), 
    freqDQM_(conf().freqDQM()), diagLevel_(conf().diag()), evtCounter_(0),
    _toJSON(conf().toJSON()) {

    book_histograms(tfs);
    update_canvas();
}

void ots::STMDQMBareRoot::beginJob() {

}

void ots::STMDQMBareRoot::beginRun(const art::Run& run) {
  // _evtNumHist->Reset();
  // _dataTypesHist->Reset();
  // _lastWaveform->Reset();
}

void ots::STMDQMBareRoot::book_histograms(art::ServiceHandle<art::TFileService> tfs) {
    _evtNumHist = tfs->make<TH1F>("event_number_recent", Form("Event Number (every %d events)", freqDQM_), 1000, 0, 10000);
    
    _dataTypesHist = tfs->make<TH1F>("stm_data_types", Form("Data Types (every %d events); data type; N fragments", freqDQM_),3,0,3);
    _dataTypesHist->GetXaxis()->SetBinLabel(1, "raw");
    _dataTypesHist->GetXaxis()->SetBinLabel(2, "ZS");
    //    _dataTypesHist->GetXaxis()->SetBinLabel(3, "raw");
    
    _lastWaveform = tfs->makeAndRegister<TGraph>("last_waveform", "Last Waveform", 300);

    _c_stmdqm = tfs->makeAndRegister<TCanvas>("c_stmdqm", "c_stmdqm");
    _c_stmdqm->Divide(2, 2);
}

void ots::STMDQMBareRoot::update_canvas() {
  _c_stmdqm->cd(1);
  _evtNumHist->Draw("");

  _c_stmdqm->cd(2);
  _dataTypesHist->Draw();

  _c_stmdqm->cd(3);
  _lastWaveform->Draw("AL");
  _lastWaveform->GetXaxis()->SetTitle("Sample Number");
  _lastWaveform->GetYaxis()->SetTitle("ADC Value");

  if (_toJSON) {
    auto before_json    = std::chrono::steady_clock::now();
    _jsonFile.open("/home/mu2estm/vst_al9/tdaq-v3_01_00/otsdaq-mu2e-stm/UserWebGUI/json_dqm/c_stmdqm.json", std::fstream::out);
    auto after_json_open    = std::chrono::steady_clock::now();
    auto json = TBufferJSON::ToJSON(_c_stmdqm);
    //  auto json = TBufferJSON::ToJSON(_evtNumHist);
    auto after_json_conv    = std::chrono::steady_clock::now();
    _jsonFile << json;
    auto after_json_write    = std::chrono::steady_clock::now();
    _jsonFile.close();
    auto after_json_close    = std::chrono::steady_clock::now();

    std::cout << "Opening = " << artdaq::TimeUtils::GetElapsedTime(before_json, after_json_open)*1000 << " ms" << std::endl;
    std::cout << "Converting = " << artdaq::TimeUtils::GetElapsedTime(after_json_open, after_json_conv)*1000 << " ms" << std::endl;
    std::cout << "Writing = " << artdaq::TimeUtils::GetElapsedTime(after_json_conv, after_json_write)*1000 << " ms" << std::endl;
    std::cout << "Closing = " << artdaq::TimeUtils::GetElapsedTime(after_json_write, after_json_close)*1000 << " ms" << std::endl;
  }
}

void ots::STMDQMBareRoot::analyze(art::Event const& event) {
  ++evtCounter_;
 
  auto const stmH   = event.getValidHandle<std::vector<artdaq::Fragment>>(moduleTag_);
  const std::vector<artdaq::Fragment>         *stmFrags = stmH.product();


  //  std::cout << "HERE: " << evtCounter_ << std::endl;

  for (const auto& frag : *stmFrags) {

    mu2e::STMFragment stmFrag = static_cast<mu2e::STMFragment>(frag);
    //      std::cout << "[STMDQMBareRoot::analyze] data = ";
      // for (int i = 0; i < 20; ++i) {
      //   std::cout << *(stmFrag.DataBegin() + i) << " ";
      // }
      // std::cout << std::endl;

    if (*(stmFrag.ZPFlag()) == 0) {
      int16_t event_number = *(stmFrag.EvNum());
      //      std::cout << "[STMDQMBareRoot::analyze] EvNum = " << event_number << std::endl;
      _evtNumHist->Fill(event_number);

      _dataTypesHist->Fill(0);

      auto waveformBegin = stmFrag.DataBegin();
      for (int i_point = 0; i_point < _lastWaveform->GetN(); ++i_point) {
        _lastWaveform->SetPoint(i_point, i_point, *(waveformBegin+i_point));
      }
    }
    else {
      _dataTypesHist->Fill(1); // zero-suppressed
    }
  }

  if (evtCounter_ % freqDQM_  != 0) return;
  update_canvas(); // update the canvas, also writes to json file
}



void ots::STMDQMBareRoot::endJob() {
  // only histograms are automatically written to the TFileService file
  //  _c_stmdqm->Write();
  //  _lastWaveform->Write();
}

DEFINE_ART_MODULE(ots::STMDQMBareRoot)
