#ifndef _STMDQMHistoContainer_h_
#define _STMDQMHistoContainer_h_

#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileDirectory.h"
#include "art_root_io/TFileService.h"
#include "otsdaq/NetworkUtilities/TCPPublishServer.h"
#include "otsdaq/Macros/CoutMacros.h"
#include <TH1F.h>
#include <string>

namespace ots {

  class STMDQMHistoContainer {
  public:
    STMDQMHistoContainer(){};
    virtual ~STMDQMHistoContainer(void){};
    struct summaryInfoHist_ {
      TH1F *_Hist;
      summaryInfoHist_() { _Hist = NULL; }
    };

    std::vector<summaryInfoHist_> histograms;

    void BookSummaryHistos(art::ServiceHandle<art::TFileService> tfs, std::string Name, std::string Title,
			   int nBins, float min, float max) {
      histograms.push_back(summaryInfoHist_());
      art::TFileDirectory testDir = tfs->mkdir("STM_summary");
      this->histograms[histograms.size() - 1]._Hist = 
	testDir.make<TH1F>(Name.c_str(), Title.c_str(), nBins, min, max);
    }
  };

} // namespace ots

#endif
