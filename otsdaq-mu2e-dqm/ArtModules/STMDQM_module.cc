// Author: A. Edmonds (based on G. Pezzullo's IntensityInfo)
// This module produces histograms of data from the STM

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art_root_io/TFileService.h"
#include "fhiclcpp/types/OptionalAtom.h"
#include "canvas/Persistency/Common/TriggerResults.h"
#include "art/Framework/Services/System/TriggerNamesService.h"

#include <TBufferFile.h>
#include <TH1F.h>

#include "otsdaq-mu2e-dqm/ArtModules/STMDQMHistoContainer.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/NetworkUtilities/TCPSendClient.h"
#include "otsdaq-mu2e/ArtModules/HistoSender.hh"

#include "artdaq-core-mu2e/Overlays/STMFragment.hh"

namespace ots {
  class STMDQM : public art::EDAnalyzer {
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
    };

    typedef art::EDAnalyzer::Table<Config> Parameters;

    explicit STMDQM(Parameters const& conf);

    void analyze(art::Event const& event) override;
    void beginRun(art::Run const&) override;
    void beginJob() override;
    void endJob() override;

    void summary_fill(STMDQMHistoContainer *recent_histos, 
		      const std::vector<artdaq::Fragment>        *stmFrag);
    void PlotRate(art::Event const& e);

  private:
    Config                    conf_;
    int                       port_;
    std::string               address_;
    std::string               moduleTag_;
    std::vector<std::string>  histType_;
    int                       freqDQM_,  diagLevel_, evtCounter_;
    art::ServiceHandle<art::TFileService> tfs;
    STMDQMHistoContainer* recent_histos  = new STMDQMHistoContainer();
    HistoSender*              histSender_;
    bool                      doOnspillHist_, doOffspillHist_;
    
  };
} // namespace ots

ots::STMDQM::STMDQM(Parameters const& conf)
  : art::EDAnalyzer(conf), conf_(conf()), port_(conf().port()), address_(conf().address()),
    moduleTag_(conf().moduleTag()), histType_(conf().histType()), 
    freqDQM_(conf().freqDQM()), diagLevel_(conf().diag()), evtCounter_(0), 
    doOnspillHist_(false), doOffspillHist_(false) {
  histSender_  = new HistoSender(address_, port_);
  
  if (diagLevel_>0){
    __MOUT__ << "[STMDQM::analyze] DQM for "<< histType_[0] << std::endl;
  }

  for (std::string name : histType_) {
    if (name == "Onspill") {
      doOnspillHist_ = true;
    }
    if (name == "Offspill") {
      doOffspillHist_ = true;
    }
  }
}

void ots::STMDQM::beginJob() {
  std::cout << "[STMDQM::beginJob] Beginning job" << std::endl;
  recent_histos->BookSummaryHistos(tfs,
				   "event_number_recent", Form("Event Number (every %d events)", freqDQM_),
				   10000, 0, 10000);

  recent_histos->BookSummaryHistos(tfs,
				   "stm_data_types", Form("Data Types (every %d events); data type; N fragments", freqDQM_),
                                   3,0,3);
  recent_histos->histograms[1]._Hist->GetXaxis()->SetBinLabel(1, "raw");
  recent_histos->histograms[1]._Hist->GetXaxis()->SetBinLabel(2, "ZS");
  recent_histos->histograms[1]._Hist->GetXaxis()->SetBinLabel(3, "MWD");

  recent_histos->BookSummaryHistos(tfs,
				   "stm_data_types2", Form("Data Types2 (every %d events); data type; N fragments", freqDQM_),
                                   100,0,100);
}

void ots::STMDQM::analyze(art::Event const& event) {
  ++evtCounter_;
 
  auto const stmH   = event.getValidHandle<std::vector<artdaq::Fragment>>(moduleTag_);
  const std::vector<artdaq::Fragment>         *stmFrags = stmH.product();

  summary_fill(recent_histos, stmFrags);
  

  if (evtCounter_ % freqDQM_  != 0) return;

  std::cout << "HERE: " << evtCounter_ << std::endl;

  //send a packet AND reset the histograms
  std::map<std::string,std::vector<TH1*>>   hists_to_send;
  
  // send the recent histos and reset them
  for (size_t i = 0; i < recent_histos->histograms.size(); i++) {
    std::cout << "[STMDQM::analyze] collecting recent histogram "<< recent_histos->histograms[i]._Hist << std::endl;
    // std::cout << "Bin Contents: " << std::endl << "\t";
    // for (int i_bin = 1; i_bin <= recent_histos->histograms[i]._Hist->GetNbinsX(); ++i_bin) {
    //   std::cout << recent_histos->histograms[i]._Hist->GetBinContent(i_bin) << " ";
    // }
    // std::cout << std::endl;
    auto clone = (TH1*)recent_histos->histograms[i]._Hist->Clone();
    // std::cout << "Cloned Bin Contents:" << std::endl << "\t";
    // for (int i_bin = 1; i_bin <= clone->GetNbinsX(); ++i_bin) {
    //   std::cout << clone->GetBinContent(i_bin) << " ";
    // }
    // std::cout << std::endl;
    std::cout << "Pushing back clone = " << clone << std::endl;
    hists_to_send["STM_summary"].push_back(clone);
    recent_histos->histograms[i]._Hist->Reset();
    std::cout << "[STMDQM::analyze] after pushing back and Reset, hist " << i << " has " << recent_histos->histograms[i]._Hist->GetEntries() << " entries" << std::endl;
  }

  histSender_->sendHistograms(hists_to_send);
}


void ots::STMDQM::summary_fill(STMDQMHistoContainer       *recent_histos,
			       const std::vector<artdaq::Fragment>        *stmFrags) {
  std::cout << "[STMDQM::summary_fill] filling Summary histograms..."<< std::endl;

  if (recent_histos->histograms.size() == 0) {
    std::cout << "No histograms booked. Should they have been created elsewhere?"
	      << std::endl;
  } else {
      
  //   // Used to get the number of triggered events from each trigger path
    for (const auto& frag : *stmFrags) {
  //     std::cout << "AE: Test Word = " << std::hex << static_cast<mu2e::STMFragment>(frag).GetTHdr()->testWord() << std::endl;
  //     std::cout << "AE: Trigger Number = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetTHdr()->triggerNumber() << std::endl;
  //     std::cout << "AE: Mode, Channel, Type = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetTHdr()->mode() << ", " << static_cast<mu2e::STMFragment>(frag).GetTHdr()->channel() << ", " << static_cast<mu2e::STMFragment>(frag).GetTHdr()->type() <<  std::endl;
  //     std::cout << "AE: Slice Number = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetTHdr()->sliceNumber() << std::endl;
  //     std::cout << "AE: Trigger Time = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetTHdr()->triggerTime() << std::endl;
  //     std::cout << "AE: ADC Offset = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetTHdr()->adcOffset() << std::endl;
  //     std::cout << "AE: Unix Time = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetTHdr()->unixTime() << std::endl;
  //     std::cout << "AE: Dropped Packets = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetTHdr()->droppedPackets() << std::endl;

  //     std::cout << "AE: Slice Number = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetSHdr()->sliceNumber() << std::endl;
  //     std::cout << "AE: Slice Size = " << std::dec << static_cast<mu2e::STMFragment>(frag).GetSHdr()->sliceSize() << std::endl;

      mu2e::STMFragment stmFrag = static_cast<mu2e::STMFragment>(frag);
      //      TLOG_DEBUG(3) << stmFrag;
      //      int16_t const* stmDataBegin = reinterpret_cast<int16_t const*>(frag.dataBegin());
      std::cout << "[STMDQM::summary_fill] data = ";
      for (int i = 0; i < 20; ++i) {
        std::cout << *(stmFrag.DataBegin() + i) << " ";
      }
      std::cout << std::endl;

      int16_t event_number = *(stmFrag.EvNum());
      std::cout << "[STMDQM::summary_fill] EvNum = " << event_number << std::endl;
      recent_histos->histograms[0]._Hist->Fill(event_number);

      int16_t data_type = *(stmFrag.ZPFlag());
      recent_histos->histograms[1]._Hist->Fill(data_type);

      std::cout << "[STMDQM::summary_fill] data_type hist has " << recent_histos->histograms[1]._Hist->GetEntries() << " entries" << std::endl;
    }
  }
}

void ots::STMDQM::endJob() {}

void ots::STMDQM::beginRun(const art::Run& run) {}

DEFINE_ART_MODULE(ots::STMDQM)
